// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_process/service_process_control.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/service_process_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

using content::BrowserThread;

namespace {

// The number of and initial delay between retry attempts when connecting to the
// service process. These are applied with exponential backoff and are necessary
// to avoid inherent raciness in how the service process listens for incoming
// connections, particularly on Windows.
const size_t kMaxConnectionAttempts = 10;
constexpr base::TimeDelta kInitialConnectionRetryDelay =
    base::TimeDelta::FromMilliseconds(20);

void ConnectAsyncWithBackoff(
    service_manager::mojom::InterfaceProviderRequest interface_provider_request,
    mojo::NamedPlatformChannel::ServerName server_name,
    size_t num_retries_left,
    base::TimeDelta retry_delay,
    scoped_refptr<base::TaskRunner> response_task_runner,
    base::OnceCallback<void(std::unique_ptr<mojo::IsolatedConnection>)>
        response_callback) {
  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    if (num_retries_left == 0) {
      response_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(response_callback), nullptr));
    } else {
      base::PostDelayedTaskWithTraits(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(
              &ConnectAsyncWithBackoff, std::move(interface_provider_request),
              server_name, num_retries_left - 1, retry_delay * 2,
              std::move(response_task_runner), std::move(response_callback)),
          retry_delay);
    }
  } else {
    auto mojo_connection = std::make_unique<mojo::IsolatedConnection>();
    mojo::FuseMessagePipes(mojo_connection->Connect(std::move(endpoint)),
                           interface_provider_request.PassMessagePipe());
    response_task_runner->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(response_callback),
                                                  std::move(mojo_connection)));
  }
}

}  // namespace

// ServiceProcessControl implementation.
ServiceProcessControl::ServiceProcessControl()
    : apply_changes_from_upgrade_observer_(false), weak_factory_(this) {
  UpgradeDetector::GetInstance()->AddObserver(this);
}

ServiceProcessControl::~ServiceProcessControl() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

void ServiceProcessControl::ConnectInternal() {
  // If the channel has already been established then we run the task
  // and return.
  if (service_process_) {
    RunConnectDoneTasks();
    return;
  }

  // Actually going to connect.
  DVLOG(1) << "Connecting to Service Process IPC Server";

  service_manager::mojom::InterfaceProviderPtr remote_interfaces;
  auto interface_provider_request = mojo::MakeRequest(&remote_interfaces);
  SetMojoHandle(std::move(remote_interfaces));
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &ConnectAsyncWithBackoff, std::move(interface_provider_request),
          GetServiceProcessServerName(), kMaxConnectionAttempts,
          kInitialConnectionRetryDelay, base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&ServiceProcessControl::OnPeerConnectionComplete,
                         weak_factory_.GetWeakPtr())));
}

void ServiceProcessControl::OnPeerConnectionComplete(
    std::unique_ptr<mojo::IsolatedConnection> connection) {
  // Hold onto the connection object so the connection is kept alive.
  mojo_connection_ = std::move(connection);
}

void ServiceProcessControl::SetMojoHandle(
    service_manager::mojom::InterfaceProviderPtr handle) {
  remote_interfaces_.Close();
  remote_interfaces_.Bind(std::move(handle));
  remote_interfaces_.SetConnectionLostClosure(base::Bind(
      &ServiceProcessControl::OnChannelError, base::Unretained(this)));

  // TODO(hclam): Handle error connecting to channel.
  remote_interfaces_.GetInterface(&service_process_);
  service_process_->Hello(base::BindOnce(
      &ServiceProcessControl::OnChannelConnected, base::Unretained(this)));
}

void ServiceProcessControl::RunConnectDoneTasks() {
  // The tasks executed here may add more tasks to the vector. So copy
  // them to the stack before executing them. This way recursion is
  // avoided.
  TaskList tasks;

  if (IsConnected()) {
    tasks.swap(connect_success_tasks_);
    RunAllTasksHelper(&tasks);
    DCHECK(tasks.empty());
    connect_failure_tasks_.clear();
  } else {
    tasks.swap(connect_failure_tasks_);
    RunAllTasksHelper(&tasks);
    DCHECK(tasks.empty());
    connect_success_tasks_.clear();
  }
}

// static
void ServiceProcessControl::RunAllTasksHelper(TaskList* task_list) {
  auto index = task_list->begin();
  while (index != task_list->end()) {
    std::move(*index).Run();
    index = task_list->erase(index);
  }
}

bool ServiceProcessControl::IsConnected() const {
  return !!service_process_;
}

void ServiceProcessControl::Launch(base::OnceClosure success_task,
                                   base::OnceClosure failure_task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (success_task)
    connect_success_tasks_.emplace_back(std::move(success_task));

  if (failure_task)
    connect_failure_tasks_.emplace_back(std::move(failure_task));

  // If we already in the process of launching, then we are done.
  if (launcher_.get())
    return;

  // If the service process is already running then connects to it.
  if (CheckServiceProcessReady()) {
    ConnectInternal();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents", SERVICE_EVENT_LAUNCH,
                            SERVICE_EVENT_MAX);

  std::unique_ptr<base::CommandLine> cmd_line(
      CreateServiceProcessCommandLine());
  // And then start the process asynchronously.
  launcher_ = new Launcher(std::move(cmd_line));
  launcher_->Run(base::Bind(&ServiceProcessControl::OnProcessLaunched,
                            base::Unretained(this)));
}

void ServiceProcessControl::Disconnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo_connection_.reset();
  remote_interfaces_.Close();
  service_process_.reset();
}

void ServiceProcessControl::OnProcessLaunched() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (launcher_->launched()) {
    saved_pid_ = launcher_->saved_pid();
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                              SERVICE_EVENT_LAUNCHED, SERVICE_EVENT_MAX);
    // After we have successfully created the service process we try to connect
    // to it. The launch task is transfered to a connect task.
    ConnectInternal();
  } else {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                              SERVICE_EVENT_LAUNCH_FAILED, SERVICE_EVENT_MAX);
    // If we don't have process handle that means launching the service process
    // has failed.
    RunConnectDoneTasks();
  }

  // We don't need the launcher anymore.
  launcher_ = NULL;
}

void ServiceProcessControl::OnUpgradeRecommended() {
  if (apply_changes_from_upgrade_observer_)
    service_process_->UpdateAvailable();
}

void ServiceProcessControl::OnChannelConnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_CHANNEL_CONNECTED, SERVICE_EVENT_MAX);

  // We just established a channel with the service process. Notify it if an
  // upgrade is available.
  if (UpgradeDetector::GetInstance()->notify_upgrade())
    service_process_->UpdateAvailable();
  else
    apply_changes_from_upgrade_observer_ = true;

  RunConnectDoneTasks();
}

void ServiceProcessControl::OnChannelError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_CHANNEL_ERROR, SERVICE_EVENT_MAX);

  Disconnect();
  RunConnectDoneTasks();
}

void ServiceProcessControl::OnHistograms(
    const std::vector<std::string>& pickled_histograms) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_HISTOGRAMS_REPLY, SERVICE_EVENT_MAX);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::HistogramDeltaSerialization::DeserializeAndAddSamples(
      pickled_histograms);
  RunHistogramsCallback();
}

void ServiceProcessControl::RunHistogramsCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!histograms_callback_.is_null()) {
    histograms_callback_.Run();
    histograms_callback_.Reset();
  }
  histograms_timeout_callback_.Cancel();
}

bool ServiceProcessControl::GetHistograms(
    const base::Closure& histograms_callback,
    const base::TimeDelta& timeout) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!histograms_callback.is_null());
  histograms_callback_.Reset();

#if defined(OS_MACOSX)
  // TODO(vitalybuka): Investigate why it crashes MAC http://crbug.com/406227.
  return false;
#endif  // OS_MACOSX

  // If the service process is already running then connect to it.
  if (!CheckServiceProcessReady())
    return false;
  ConnectInternal();

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_HISTOGRAMS_REQUEST,
                            SERVICE_EVENT_MAX);

  if (!service_process_)
    return false;

  service_process_->GetHistograms(base::BindOnce(
      &ServiceProcessControl::OnHistograms, base::Unretained(this)));

  // Run timeout task to make sure |histograms_callback| is called.
  histograms_timeout_callback_.Reset(base::Bind(
      &ServiceProcessControl::RunHistogramsCallback, base::Unretained(this)));
  base::PostDelayedTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                                  histograms_timeout_callback_.callback(),
                                  timeout);

  histograms_callback_ = histograms_callback;
  return true;
}

bool ServiceProcessControl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_process_)
    return false;
  service_process_->ShutDown();
  Disconnect();
  return true;
}

// static
ServiceProcessControl* ServiceProcessControl::GetInstance() {
  return base::Singleton<ServiceProcessControl>::get();
}

ServiceProcessControl::Launcher::Launcher(
    std::unique_ptr<base::CommandLine> cmd_line)
    : cmd_line_(std::move(cmd_line)), launched_(false), retry_count_(0) {}

// Execute the command line to start the process asynchronously.
// After the command is executed, |task| is called with the process handle on
// the UI thread.
void ServiceProcessControl::Launcher::Run(const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notify_task_ = task;
  content::GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Launcher::DoRun, this));
}

ServiceProcessControl::Launcher::~Launcher() {
}


void ServiceProcessControl::Launcher::Notify() {
  DCHECK(!notify_task_.is_null());
  notify_task_.Run();
  notify_task_.Reset();
}

#if !defined(OS_MACOSX)
void ServiceProcessControl::Launcher::DoDetectLaunched() {
  DCHECK(!notify_task_.is_null());

  const uint32_t kMaxLaunchDetectRetries = 10;
  launched_ = CheckServiceProcessReady();

  int exit_code = 0;
  if (launched_ || (retry_count_ >= kMaxLaunchDetectRetries) ||
      process_.WaitForExitWithTimeout(base::TimeDelta(), &exit_code)) {
    process_.Close();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::Bind(&Launcher::Notify, this));
    return;
  }
  retry_count_++;

  // If the service process is not launched yet then check again in 2 seconds.
  const base::TimeDelta kDetectLaunchRetry = base::TimeDelta::FromSeconds(2);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&Launcher::DoDetectLaunched, this),
      kDetectLaunchRetry);
}

void ServiceProcessControl::Launcher::DoRun() {
  DCHECK(!notify_task_.is_null());

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  process_ = base::LaunchProcess(*cmd_line_, options);
  if (process_.IsValid()) {
    saved_pid_ = process_.Pid();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::Bind(&Launcher::DoDetectLaunched, this));
  } else {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::Bind(&Launcher::Notify, this));
  }
}
#endif  // !OS_MACOSX

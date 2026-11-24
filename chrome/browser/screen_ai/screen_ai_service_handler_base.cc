// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_handler_base.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_library_provider.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/screen_ai/public/cpp/utilities.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// If any value is added, please update `ComponentAvailability` in `enums.xml`.
enum class ComponentAvailability {
  kAvailable = 0,
  kUnavailableWithNetwork = 1,
  kUnavailableWithoutNetwork = 2,

  kMaxValue = kUnavailableWithoutNetwork,
};

void RecordComponentAvailability(bool available) {
  ComponentAvailability availability = ComponentAvailability::kAvailable;
  if (!available) {
    availability = content::GetNetworkConnectionTracker()->IsOffline()
                       ? ComponentAvailability::kUnavailableWithoutNetwork
                       : ComponentAvailability::kUnavailableWithNetwork;
  }
  base::UmaHistogramEnumeration("Accessibility.ScreenAI.Component.Available2",
                                availability);
}

}  // namespace

namespace screen_ai {

bool IsModelFileContentReadable(base::File& file) {
  if (!file.IsValid()) {
    return false;
  }
  int64_t file_size = file.GetLength();
  if (file_size <= 0) {
    return false;
  }
  std::vector<uint8_t> buffer(base::checked_cast<size_t>(file_size));
  return file.ReadAndCheck(0, base::span(buffer));
}

ComponentFiles::ComponentFiles(
    const base::FilePath& library_binary_path,
    const base::FilePath::CharType* files_list_file_name)
    : library_binary_path_(library_binary_path) {
  base::FilePath component_folder = library_binary_path.DirName();

  // Get the files list.
  std::string file_content;
  if (!base::ReadFileToString(component_folder.Append(files_list_file_name),
                              &file_content)) {
    VLOG(0) << "Could not read list of files for " << files_list_file_name;
    return;
  }
  std::vector<std::string_view> files_list = base::SplitStringPiece(
      file_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (files_list.empty()) {
    VLOG(0) << "Could not parse files list for " << files_list_file_name;
    return;
  }

  for (std::string_view relative_file_path : files_list) {
    // Ignore comment lines.
    if (relative_file_path.empty() || relative_file_path[0] == '#') {
      continue;
    }

#if BUILDFLAG(IS_WIN)
    base::FilePath relative_path(base::UTF8ToWide(relative_file_path));
#else
    base::FilePath relative_path(relative_file_path);
#endif
    // The files list comes from the disk. Do not let it point outside of the
    // component folder.
    if (relative_path.IsAbsolute() || relative_path.ReferencesParent()) {
      VLOG(0) << "Invalid file path in " << files_list_file_name;
      model_files_.clear();
      return;
    }
    const base::FilePath full_path = component_folder.Append(relative_path);
    model_files_[relative_path] =
        base::File(full_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!IsModelFileContentReadable(model_files_[relative_path])) {
      VLOG(0) << "Could not open " << full_path;
      model_files_.clear();
      return;
    }
  }
}

ComponentFiles::~ComponentFiles() {
  if (model_files_.empty()) {
    return;
  }

  // Transfer ownership of the file handles to a thread that may block, and let
  // them get destroyed there.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::DoNothingWithBoundArgs(std::move(model_files_)));
}

std::unique_ptr<ComponentFiles> ComponentFiles::Load(
    const base::FilePath& library_binary_path,
    const base::FilePath::CharType* files_list_file_name) {
  return std::make_unique<ComponentFiles>(library_binary_path,
                                          files_list_file_name);
}

ScreenAIServiceHandlerBase::ScreenAIServiceHandlerBase(
    ScreenAILibraryProvider* library_provider)
    : library_provider_(library_provider),
      screen_ai_service_shutdown_handler_(this) {
  CHECK(library_provider_);
}

ScreenAIServiceHandlerBase::~ScreenAIServiceHandlerBase() = default;

// static
// LINT.IfChange(SuggestedWaitTimeBeforeReAttempt)
base::TimeDelta ScreenAIServiceHandlerBase::SuggestedWaitTimeBeforeReAttempt(
    uint32_t reattempt_number) {
  return base::Minutes(reattempt_number * reattempt_number);
}
// LINT.ThenChange(//chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.cc:SuggestedWaitTimeBeforeReAttempt)

std::string ScreenAIServiceHandlerBase::GetMetricFullName(
    std::string_view metric_name) const {
  return base::StringPrintf("Accessibility.%s.Service.%s", GetServiceName(),
                            metric_name);
}

std::optional<bool> ScreenAIServiceHandlerBase::GetServiceState() {
  if (GetAndRecordSuspendedState()) {
    return false;
  }
  if (IsConnectionBound()) {
    return true;
  }
  if (IsServiceEnabled()) {
    return std::nullopt;
  }
  return false;
}

void ScreenAIServiceHandlerBase::GetServiceStateAsync(
    ServiceStateCallback callback) {
  auto service_state = GetServiceState();

  // If `service_state` has value, either the service is already initialized or
  // disabled. In both cases we can assume the component was ready.
  if (service_state) {
    RecordComponentAvailability(true);
    std::move(callback).Run(*service_state);
    return;
  }

  // Put the request in queue. It is answered after the library provider tells
  // if the library is available.
  pending_state_requests_.emplace_back(std::move(callback));

  // Ask for the library, unless an earlier request already waits for it.
  if (waiting_for_library_) {
    return;
  }
  waiting_for_library_ = true;
  base::WeakPtr<ScreenAIServiceHandlerBase> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  base::CallbackListSubscription subscription =
      library_provider_->EnsureLibraryAvailable(base::BindOnce(
          &ScreenAIServiceHandlerBase::OnLibraryAvailabilityChanged,
          weak_this));
  // The provider can answer before it returns. The callbacks that are then
  // told that the service is not available can destroy this object, or ask
  // for the service again, which stores a new subscription. So keep this
  // subscription only if it is still the request to wait for.
  if (weak_this && waiting_for_library_ && !library_subscription_) {
    library_subscription_ = std::move(subscription);
  }
}

void ScreenAIServiceHandlerBase::OnLibraryAvailabilityChanged(bool available) {
  waiting_for_library_ = false;
  library_subscription_ = {};
  RecordComponentAvailability(available);

  if (pending_state_requests_.empty()) {
    return;
  }

  if (available) {
    InitializeServiceIfNeeded();
  } else {
    CallPendingStatusRequests(false);
  }
}

void ScreenAIServiceHandlerBase::ShuttingDownOnIdle() {
  shutdown_handler_data_.shutdown_message_received = true;
}

bool ScreenAIServiceHandlerBase::GetAndRecordSuspendedState() {
  base::UmaHistogramBoolean(GetMetricFullName("IsSuspended"),
                            shutdown_handler_data_.suspended);
  return shutdown_handler_data_.suspended;
}

void ScreenAIServiceHandlerBase::OnScreenAIServiceDisconnected() {
  screen_ai_service_factory_.reset();
  CallPendingStatusRequests(false);

  if (resource_monitor_) {
    if (!resource_monitor_->get_max_resident_memory().is_zero()) {
      base::UmaHistogramMemoryMB(
          GetMetricFullName("MaxMemoryLoad"),
          resource_monitor_->get_max_resident_memory().InKiB() / 1000);
    }
    resource_monitor_.reset();

    base::UmaHistogramMediumTimes(GetMetricFullName("LifeTime"),
                                  base::TimeTicks::Now() - service_start_time_);
  }

  screen_ai_service_shutdown_handler_.reset();
  if (shutdown_handler_data_.shutdown_message_received) {
    if (shutdown_handler_data_.crash_count) {
      base::UmaHistogramCounts100(GetMetricFullName("CrashCountBeforeResume"),
                                  shutdown_handler_data_.crash_count);
    }
    shutdown_handler_data_.crash_count = 0;
    return;
  }

  // Crashed!
  shutdown_handler_data_.crash_count++;
  shutdown_handler_data_.suspended = true;
  base::TimeDelta suspense_time =
      SuggestedWaitTimeBeforeReAttempt(shutdown_handler_data_.crash_count);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScreenAIServiceHandlerBase::ResetSuspend,
                     weak_ptr_factory_.GetWeakPtr()),
      suspense_time);
  VLOG(0) << "Service suspended due to crash for: " << suspense_time;
}

void ScreenAIServiceHandlerBase::CallPendingStatusRequests(bool successful) {
  std::vector<ServiceStateCallback> requests;
  pending_state_requests_.swap(requests);
  for (auto& callback : requests) {
    std::move(callback).Run(successful);
  }
}

void ScreenAIServiceHandlerBase::LaunchIfNotRunning() {
  library_provider_->RecordUsage();
  if (screen_ai_service_factory_.is_bound()) {
    return;
  }

  base::FilePath binary_path = library_provider_->GetBinaryPath();

  // To have a smooth user experience, the callers of the service should ensure
  // that the component is downloaded before promising it to the users and
  // triggering its launch.
  // If it is not done, the calling feature will receive no reply when it tries
  // to use this service. However, they can detect it by using an on-disconnect
  // handler.
  if (binary_path.empty()) {
    VLOG(0) << "ScreenAI service launch triggered when component is not "
               "available.";
    // Start the download of the library. Nobody waits for it here.
    std::ignore = library_provider_->EnsureLibraryAvailable(base::DoNothing());
    return;
  }

  // The library is loaded before the sandbox is set up, so only accept a full
  // path that does not go through a parent directory.
  if (!binary_path.IsAbsolute() || binary_path.ReferencesParent()) {
    VLOG(0) << "ScreenAI service launch triggered with an invalid library "
               "path.";
    return;
  }

  if (GetAndRecordSuspendedState()) {
    VLOG(0) << "ScreenAI service triggered while suspended.";
    return;
  }

#if BUILDFLAG(IS_WIN)
  std::vector<base::FilePath> preload_libraries = {binary_path};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::vector<std::string> extra_switches = {
      base::StringPrintf("--%s=%s", screen_ai::GetBinaryPathSwitch(),
                         binary_path.MaybeAsASCII().c_str())};
#endif  // BUILDFLAG(IS_WIN)

  std::string process_name = base::StrCat({GetServiceName(), " Service"});
  content::ServiceProcessHost::Launch(
      screen_ai_service_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(process_name)
#if BUILDFLAG(IS_WIN)
          .WithPreloadedLibraries(
              preload_libraries,
              content::ServiceProcessHostPreloadLibraries::GetPassKey())
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
          .WithExtraCommandLineSwitches(extra_switches)
#endif  // BUILDFLAG(IS_WIN)
          .WithProcessCallback(
              base::BindOnce(&ScreenAIServiceHandlerBase::OnServiceLaunched,
                             weak_ptr_factory_.GetWeakPtr(), process_name))
          .Pass());

  shutdown_handler_data_.shutdown_message_received = false;
  screen_ai_service_factory_->BindShutdownHandler(
      screen_ai_service_shutdown_handler_.BindNewPipeAndPassRemote());

  screen_ai_service_factory_.set_disconnect_handler(
      base::BindOnce(&ScreenAIServiceHandlerBase::OnScreenAIServiceDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenAIServiceHandlerBase::OnServiceLaunched(
    const std::string& process_name,
    const base::Process& process) {
  // Post task to ensure that `resource_monitor_` is created after the service
  // process host is registered.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ScreenAIServiceHandlerBase::CreateResourceMonitor,
                     weak_ptr_factory_.GetWeakPtr(), process_name));
  service_start_time_ = base::TimeTicks::Now();
}

void ScreenAIServiceHandlerBase::CreateResourceMonitor(
    const std::string& process_name) {
  CHECK(!resource_monitor_);

  // Resource monitor is created with a delay after service is launched. Do not
  // create it if the service is disconnected during this delay.
  if (!screen_ai_service_factory_.is_bound()) {
    return;
  }
  // Resource monitor creation may [rarely] fail if the process name is not
  // found in process registry. It is only used for metrics and does not affect
  // user experience.
  resource_monitor_ = ResourceMonitor::CreateForProcess(process_name);
}

void ScreenAIServiceHandlerBase::InitializeServiceIfNeeded() {
  std::optional<bool> service_state = GetServiceState();
  if (service_state) {
    // Either service is already initialized or disabled.
    CallPendingStatusRequests(*service_state);
    return;
  }

  base::TimeTicks request_start_time = base::TimeTicks::Now();
  LaunchIfNotRunning();

  if (!screen_ai_service_factory_.is_bound()) {
    SetLibraryLoadState(request_start_time, false);
    return;
  }

  LoadModelFilesAndInitialize(library_provider_->GetBinaryPath(),
                              request_start_time);
}

void ScreenAIServiceHandlerBase::SetLibraryLoadState(
    base::TimeTicks request_start_time,
    bool successful) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - request_start_time;
  base::UmaHistogramBoolean(GetMetricFullName("Initialization"), successful);
  base::UmaHistogramTimes(successful
                              ? GetMetricFullName("InitializationTime.Success")
                              : GetMetricFullName("InitializationTime.Failure"),
                          elapsed_time);

  CallPendingStatusRequests(successful);

  if (!successful) {
    ResetConnection();
  }
}

bool ScreenAIServiceHandlerBase::IsConnectionBoundForTesting() {
  return IsConnectionBound();
}

bool ScreenAIServiceHandlerBase::IsProcessRunningForTesting() {
  return screen_ai_service_factory_.is_bound();
}

}  // namespace screen_ai

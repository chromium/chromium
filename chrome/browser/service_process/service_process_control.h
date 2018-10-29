// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_PROCESS_SERVICE_PROCESS_CONTROL_H_
#define CHROME_BROWSER_SERVICE_PROCESS_SERVICE_PROCESS_CONTROL_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/containers/id_map.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "chrome/common/service_process.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if defined(OS_CHROMEOS)
#error "Not supported on ChromeOS"
#endif

namespace base {
class CommandLine;
}

namespace mojo {
class IsolatedConnection;
}

// A ServiceProcessControl works as a portal between the service process and
// the browser process.
//
// It is used to start and terminate the service process. It is also used
// to send and receive IPC messages from the service process.
//
// THREADING
//
// This class is accessed on the UI thread through some UI actions.
class ServiceProcessControl : public UpgradeObserver {
 public:
  enum ServiceProcessEvent {
    SERVICE_EVENT_INITIALIZE,
    SERVICE_EVENT_ENABLED_ON_LAUNCH,
    SERVICE_EVENT_ENABLE,
    SERVICE_EVENT_DISABLE,
    SERVICE_EVENT_DISABLE_BY_POLICY,
    SERVICE_EVENT_LAUNCH,
    SERVICE_EVENT_LAUNCHED,
    SERVICE_EVENT_LAUNCH_FAILED,
    SERVICE_EVENT_CHANNEL_CONNECTED,
    SERVICE_EVENT_CHANNEL_ERROR,
    SERVICE_EVENT_INFO_REQUEST,
    SERVICE_EVENT_INFO_REPLY,
    SERVICE_EVENT_HISTOGRAMS_REQUEST,
    SERVICE_EVENT_HISTOGRAMS_REPLY,
    SERVICE_PRINTERS_REQUEST,
    SERVICE_PRINTERS_REPLY,
    SERVICE_EVENT_MAX,
  };

  // Returns the singleton instance of this class.
  static ServiceProcessControl* GetInstance();

  // Return true if this object is connected to the service.
  // Virtual for testing.
  virtual bool IsConnected() const;

  // If no service process is currently running, creates a new service process
  // and connects to it. If a service process is already running this method
  // will try to connect to it.
  // |success_task| is called when we have successfully launched the process
  // and connected to it.
  // |failure_task| is called when we failed to connect to the service process.
  // It is OK to pass the same value for |success_task| and |failure_task|. In
  // this case, the task is invoked on success or failure.
  // Note that if we are already connected to service process then
  // |success_task| can be invoked in the context of the Launch call.
  // Virtual for testing.
  virtual void Launch(base::OnceClosure success_task,
                      base::OnceClosure failure_task);

  // Disconnect the IPC channel from the service process.
  // Virtual for testing.
  virtual void Disconnect();

  // UpgradeObserver implementation.
  void OnUpgradeRecommended() override;

  // Send a shutdown message to the service process. IPC channel will be
  // destroyed after calling this method.
  // Return true if the message was sent.
  // Virtual for testing.
  virtual bool Shutdown();

  // Send request for histograms collected in service process.
  // Returns true if request was sent, and callback will be called in case of
  // success or timeout. The method resets any previous callback.
  // Returns false if service is not running or other failure, callback will not
  // be called in this case.
  bool GetHistograms(const base::Closure& cloud_print_status_callback,
                     const base::TimeDelta& timeout);

  service_manager::InterfaceProvider& remote_interfaces() {
    return remote_interfaces_;
  }

  base::ProcessId GetLaunchedPidForTesting() const { return saved_pid_; }

 private:
  // This class is responsible for launching the service process on the
  // PROCESS_LAUNCHER thread.
  class Launcher
      : public base::RefCountedThreadSafe<ServiceProcessControl::Launcher> {
   public:
    explicit Launcher(std::unique_ptr<base::CommandLine> cmd_line);
    // Execute the command line to start the process asynchronously. After the
    // command is executed |task| is called with the process handle on the UI
    // thread.
    void Run(const base::Closure& task);

    bool launched() const { return launched_; }
    base::ProcessId saved_pid() const { return saved_pid_; }

   private:
    friend class base::RefCountedThreadSafe<ServiceProcessControl::Launcher>;
    virtual ~Launcher();

#if !defined(OS_MACOSX)
    void DoDetectLaunched();
#endif  // !OS_MACOSX

    void DoRun();
    void Notify();
    std::unique_ptr<base::CommandLine> cmd_line_;
    base::Closure notify_task_;
    bool launched_;
    uint32_t retry_count_;
    base::Process process_;

    // Used to save the process id for |process_| upon successful launch.
    // Only used for testing.
    base::ProcessId saved_pid_;
  };

  friend class MockServiceProcessControl;
  friend class CloudPrintProxyPolicyStartupTest;
  friend class TestCloudPrintProxyService;

  ServiceProcessControl();
  ~ServiceProcessControl() override;

  friend struct base::DefaultSingletonTraits<ServiceProcessControl>;

  using TaskList = std::vector<base::OnceClosure>;

  void OnChannelConnected();
  void OnChannelError();

  void OnHistograms(const std::vector<std::string>& pickled_histograms);

  // Runs callback provided in |GetHistograms()|.
  void RunHistogramsCallback();

  // Helper method to invoke all the callbacks based on success or failure.
  void RunConnectDoneTasks();

  // Method called by Launcher when the service process is launched.
  void OnProcessLaunched();

  // Used internally to connect to the service process.
  void ConnectInternal();

  // Called when ConnectInternal's async work is done.
  void OnPeerConnectionComplete(
      std::unique_ptr<mojo::IsolatedConnection> connection);

  // Split out for testing.
  void SetMojoHandle(service_manager::mojom::InterfaceProviderPtr handle);

  static void RunAllTasksHelper(TaskList* task_list);

  std::unique_ptr<mojo::IsolatedConnection> mojo_connection_;

  service_manager::InterfaceProvider remote_interfaces_;
  chrome::mojom::ServiceProcessPtr service_process_;

  // Service process launcher.
  scoped_refptr<Launcher> launcher_;

  // Callbacks that get invoked when the channel is successfully connected.
  TaskList connect_success_tasks_;
  // Callbacks that get invoked when there was a connection failure.
  TaskList connect_failure_tasks_;

  // Callback that gets invoked when a message with histograms is received from
  // the service process.
  base::Closure histograms_callback_;

  // Callback that gets invoked if service didn't reply in time.
  base::CancelableClosure histograms_timeout_callback_;

  // If true changes to UpgradeObserver are applied, if false they are ignored.
  bool apply_changes_from_upgrade_observer_;

  // Same as |Launcher::saved_pid_|.
  base::ProcessId saved_pid_;

  base::WeakPtrFactory<ServiceProcessControl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessControl);
};

#endif  // CHROME_BROWSER_SERVICE_PROCESS_SERVICE_PROCESS_CONTROL_H_

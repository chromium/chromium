// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_BASE_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_BASE_H_

#include <optional>
#include <set>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/screen_ai/resource_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_factory.mojom.h"

namespace base {
class Process;
}

namespace screen_ai {

using ServiceStateCallback = base::OnceCallback<void(bool)>;

// Base class for ScreenAI service management.
class ScreenAIServiceHandlerBase
    : screen_ai::mojom::ScreenAIServiceShutdownHandler {
 public:
  ScreenAIServiceHandlerBase();
  ScreenAIServiceHandlerBase(const ScreenAIServiceHandlerBase&) = delete;
  ScreenAIServiceHandlerBase& operator=(const ScreenAIServiceHandlerBase&) =
      delete;
  ~ScreenAIServiceHandlerBase() override;

  // Schedules library download and initializes the service if needed, and
  // calls `callback` with initialization result when service is ready or
  // failed to initialize.
  // Returns the already known state of the service (see `GetServiceState`).
  std::optional<bool> GetServiceStateAsync(ServiceStateCallback callback);

  // Called when library availability state is changed.
  void OnLibraryAvailablityChanged(bool available);

  // screen_ai::mojom::ScreenAIServiceShutdownHandler::
  void ShuttingDownOnIdle() override;

  // Returns true if the connection for the service is bound.
  bool IsConnectionBoundForTesting();

  // Returns true if sandboxed process is running.
  bool IsProcessRunningForTesting();

 protected:
  std::string GetMetricFullName(std::string_view metric_name) const;

  // Initialzies the service if it's not already done.
  void InitializeServiceIfNeeded();

  // Callback from Screen AI service with library load result.
  void SetLibraryLoadState(base::TimeTicks request_start_time, bool successful);

  mojo::Remote<mojom::ScreenAIServiceFactory>& screen_ai_service_factory() {
    return screen_ai_service_factory_;
  }

 private:
  friend class ScreenAIServiceRouterFactory;
  friend class ScreenAIServiceShutdownHandlerTest;

  bool GetAndRecordSuspendedState();
  void ResetSuspend() { shutdown_handler_data_.suspended = false; }

  // Launches the service if it's not already launched.
  void LaunchIfNotRunning();

  // Called after service is launched.
  void OnServiceLaunched(const std::string& process_name,
                         const base::Process& process);

  // Creates resource monitor to track CPU and memory load.
  void CreateResourceMonitor(const std::string& process_name);

  // True if service is already initialized, false if it is disabled, and
  // nullopt if not known.
  std::optional<bool> GetServiceState();

  // Calls back all pendnding service state requests.
  void CallPendingStatusRequests(bool successful);

  // Called when ScreenAI service factory is disconnected.
  void OnScreenAIServiceDisconnected();

  // Returns the service name.
  virtual std::string GetServiceName() const = 0;

  // Loads the library model files and initializes the service.
  virtual void LoadModelFilesAndInitialize(
      base::TimeTicks request_start_time) = 0;

  virtual bool IsConnectionBound() const = 0;
  virtual bool IsServiceEnabled() const = 0;
  virtual void ResetConnection() = 0;

  // Pending requests to receive service state for each service type.
  std::vector<ServiceStateCallback> pending_state_requests_;

  std::unique_ptr<ResourceMonitor> resource_monitor_;
  struct ShutdownHandlerData {
    bool shutdown_message_received = false;
    bool suspended = false;
    int crash_count = 0;
  } shutdown_handler_data_;

  base::TimeTicks service_start_time_;

  mojo::Receiver<screen_ai::mojom::ScreenAIServiceShutdownHandler>
      screen_ai_service_shutdown_handler_;

  mojo::Remote<mojom::ScreenAIServiceFactory> screen_ai_service_factory_;

  base::WeakPtrFactory<ScreenAIServiceHandlerBase> weak_ptr_factory_{this};
};

class ComponentFiles {
 public:
  explicit ComponentFiles(const base::FilePath& library_binary_path,
                          const base::FilePath::CharType* files_list_file_name);
  ComponentFiles(const ComponentFiles&) = delete;
  ComponentFiles& operator=(const ComponentFiles&) = delete;
  ~ComponentFiles();

  static std::unique_ptr<ComponentFiles> Load(
      const base::FilePath::CharType* files_list_file_name);

  base::flat_map<base::FilePath, base::File> model_files_;
  base::FilePath library_binary_path_;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_BASE_H_

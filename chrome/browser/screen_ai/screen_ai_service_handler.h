// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_H_

#include <optional>
#include <set>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_factory.mojom.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace screen_ai {

class ComponentFiles;

using ServiceStateCallback = base::OnceCallback<void(bool)>;

// TODO(crbug.com/408174918): Rename this class to `ScreenAIServiceHandlerBase`,
// make all functions specific to OCR or MCE virtual, and create two separate
// classes for OCR and MCE.
class ScreenAIServiceHandler
    : screen_ai::mojom::ScreenAIServiceShutdownHandler {
 public:
  explicit ScreenAIServiceHandler(bool is_ocr);
  ScreenAIServiceHandler(const ScreenAIServiceHandler&) = delete;
  ScreenAIServiceHandler& operator=(const ScreenAIServiceHandler&) = delete;
  ~ScreenAIServiceHandler() override;

  void BindScreenAIAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver);

  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver);

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

 private:
  friend class ScreenAIServiceRouterFactory;
  friend class ScreenAIServiceShutdownHandlerTest;

  std::string GetMetricFullName(std::string_view metric_name) const;

  bool GetAndRecordSuspendedState();
  void ResetSuspend() { shutdown_handler_data_.suspended = false; }

  // Initialzies the service if it's not already done.
  void InitializeServiceIfNeeded();

  void InitializeOCR(base::TimeTicks request_start_time,
                     mojo::PendingReceiver<mojom::OCRService> receiver,
                     std::unique_ptr<ComponentFiles> model_files);

  void InitializeMainContentExtraction(
      base::TimeTicks request_start_time,
      mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
      std::unique_ptr<ComponentFiles> model_files);

  // Launches the service if it's not already launched.
  void LaunchIfNotRunning();

  // True if service is already initialized, false if it is disabled, and
  // nullopt if not known.
  std::optional<bool> GetServiceState();

  // Callback from Screen AI service with library load result.
  void SetLibraryLoadState(base::TimeTicks request_start_time, bool successful);

  // Calls back all pendnding service state requests.
  void CallPendingStatusRequests(bool successful);

  // Called when ScreenAI service factory is disconnected.
  void OnScreenAIServiceDisconnected();

  // Records memory metrics when service shutsdown or crashes.
  void RecordMemoryMetrics(bool crashed);

  // Pending requests to receive service state for each service type.
  std::vector<ServiceStateCallback> pending_state_requests_;

  struct ShutdownHandlerData {
    bool shutdown_message_received = false;
    bool suspended = false;
    int crash_count = 0;
  } shutdown_handler_data_;

  struct MemoryStatsBeforeLaunch {
    int total_memory;      // in MB.
    int available_memory;  // in MB.
    bool pressure_available;
    base::MemoryPressureListener::MemoryPressureLevel pressure_level;
  } memory_stats_before_launch_;

  bool ocr_initialized_ = false;

  mojo::Receiver<screen_ai::mojom::ScreenAIServiceShutdownHandler>
      screen_ai_service_shutdown_handler_;

  mojo::Remote<mojom::ScreenAIServiceFactory> screen_ai_service_factory_;

  // Bassed on the value of `is_ocr_`, only one of the following two connections
  // will be connected.
  // TODO(crbug.com/408174918): Split this class into two.
  mojo::Remote<mojom::OCRService> ocr_service_;
  mojo::Remote<mojom::MainContentExtractionService>
      main_content_extraction_service_;

  const bool is_ocr_;
  base::WeakPtrFactory<ScreenAIServiceHandler> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_H_

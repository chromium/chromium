// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_

#include <optional>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_handler_main_content_extraction.h"
#include "chrome/browser/screen_ai/screen_ai_service_handler_ocr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace screen_ai {

// TODO(crbug.com/417378344): Split this class and its factory to two separate
// classes for OCR and main content extraction.
class ScreenAIServiceRouter : public KeyedService,
                              ScreenAIInstallState::Observer {
 public:
  enum class Service {
    kMainContentExtraction,
    kOCR,
  };

  ScreenAIServiceRouter(const ScreenAIServiceRouter&) = delete;
  ScreenAIServiceRouter& operator=(const ScreenAIServiceRouter&) = delete;
  ~ScreenAIServiceRouter() override;

  // Static method to return suggested wait time before next reconnect attempt.
  static base::TimeDelta SuggestedWaitTimeBeforeReAttempt(
      uint32_t reattempt_number);

  void BindScreenAIAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver);

  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver);

  // Schedules library download and initializaes the service if needed, and
  // calls `callback` with initialization result when service is ready or
  // failed to initialize.
  void GetServiceStateAsync(Service service, ServiceStateCallback callback);

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override;

  // Returns true if the connection for `service` is bound.
  bool IsConnectionBoundForTesting(Service service);

  // Returns true if sandboxed process is running.
  bool IsProcessRunningForTesting(Service service);

 private:
  friend class ScreenAIServiceRouterFactory;

  ScreenAIServiceRouter();

  ScreenAIServiceHandlerBase* GetHandler(Service service);

  std::unique_ptr<ScreenAIServiceHandlerOCR> ocr_handler_;
  std::unique_ptr<ScreenAIServiceHandlerMainContentExtraction> mce_handler_;

  // Observes changes in Screen AI component download state.
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_ready_observer_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_

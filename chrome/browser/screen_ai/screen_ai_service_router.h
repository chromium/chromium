// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/screen_ai/main_content_extraction_service_handler.h"
#include "chrome/browser/screen_ai/ocr_service_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace screen_ai {

class ScreenAILibraryProvider;

// TODO(crbug.com/417378344): Split this class and its factory to two separate
// classes for OCR and main content extraction.
class ScreenAIServiceRouter : public KeyedService {
 public:
  enum class Service {
    kMainContentExtraction,
    kOCR,
  };

  ScreenAIServiceRouter(const ScreenAIServiceRouter&) = delete;
  ScreenAIServiceRouter& operator=(const ScreenAIServiceRouter&) = delete;
  ~ScreenAIServiceRouter() override;

  void BindScreenAIAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver);

  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver);

  // Schedules library download and initializaes the service if needed, and
  // calls `callback` with initialization result when service is ready or
  // failed to initialize.
  void GetServiceStateAsync(Service service, ServiceStateCallback callback);

  // Returns true if the connection for `service` is bound.
  bool IsConnectionBoundForTesting(Service service);

  // Returns true if sandboxed process is running.
  bool IsProcessRunningForTesting(Service service);

 private:
  friend class ScreenAIServiceRouterFactory;

  // `library_provider` must not be null and must outlive this object.
  explicit ScreenAIServiceRouter(ScreenAILibraryProvider* library_provider);

  ScreenAIServiceHandlerBase* GetHandler(Service service);

  // Gives the handlers access to the library. Not owned: the provider outlives
  // this object. In production it is a singleton that is never destroyed.
  const raw_ptr<ScreenAILibraryProvider> library_provider_;

  std::unique_ptr<OcrServiceHandler> ocr_handler_;
  std::unique_ptr<MainContentExtractionServiceHandler> mce_handler_;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_ROUTER_H_

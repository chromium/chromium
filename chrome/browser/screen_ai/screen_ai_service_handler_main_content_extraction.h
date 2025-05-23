// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_MAIN_CONTENT_EXTRACTION_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_MAIN_CONTENT_EXTRACTION_H_

#include <optional>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/screen_ai/screen_ai_service_handler_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace screen_ai {

class ScreenAIServiceHandlerMainContentExtraction
    : public ScreenAIServiceHandlerBase {
 public:
  ScreenAIServiceHandlerMainContentExtraction();
  ScreenAIServiceHandlerMainContentExtraction(
      const ScreenAIServiceHandlerMainContentExtraction&) = delete;
  ScreenAIServiceHandlerMainContentExtraction& operator=(
      const ScreenAIServiceHandlerMainContentExtraction&) = delete;
  ~ScreenAIServiceHandlerMainContentExtraction() override;

  void BindService(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver);

 private:
  void LoadModelFilesAndInitialize(base::TimeTicks request_start_time) override;

  std::string GetServiceName() const override;
  bool IsConnectionBound() const override;
  bool IsServiceEnabled() const override;
  void ResetConnection() override;

  void InitializeService(
      base::TimeTicks request_start_time,
      mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
      std::unique_ptr<ComponentFiles> model_files);

  mojo::Remote<mojom::MainContentExtractionService> service_;

  base::WeakPtrFactory<ScreenAIServiceHandlerMainContentExtraction>
      weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_SERVICE_HANDLER_MAIN_CONTENT_EXTRACTION_H_

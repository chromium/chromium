// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_library_provider.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace screen_ai {

ScreenAIServiceRouter::ScreenAIServiceRouter(
    ScreenAILibraryProvider* library_provider)
    : library_provider_(library_provider) {
  CHECK(library_provider_);
}

ScreenAIServiceRouter::~ScreenAIServiceRouter() = default;

ScreenAIServiceHandlerBase* ScreenAIServiceRouter::GetHandler(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      if (!mce_handler_) {
        mce_handler_ = std::make_unique<MainContentExtractionServiceHandler>(
            library_provider_);
      }
      return mce_handler_.get();

    case Service::kOCR:
      if (!ocr_handler_) {
        ocr_handler_ = std::make_unique<OcrServiceHandler>(library_provider_);
      }
      return ocr_handler_.get();
  }
}

void ScreenAIServiceRouter::GetServiceStateAsync(
    Service service,
    ServiceStateCallback callback) {
  GetHandler(service)->GetServiceStateAsync(std::move(callback));
}

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  // Ensure handler exists.
  GetHandler(Service::kOCR);
  ocr_handler_->BindService(std::move(receiver));
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  // Ensure handler exists.
  GetHandler(Service::kMainContentExtraction);
  mce_handler_->BindService(std::move(receiver));
}

bool ScreenAIServiceRouter::IsConnectionBoundForTesting(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      return mce_handler_ &&
             mce_handler_->IsConnectionBoundForTesting();  // IN-TEST
    case Service::kOCR:
      return ocr_handler_ &&
             ocr_handler_->IsConnectionBoundForTesting();  // IN-TEST
  }
}

bool ScreenAIServiceRouter::IsProcessRunningForTesting(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      return mce_handler_ &&
             mce_handler_->IsProcessRunningForTesting();  // IN-TEST
    case Service::kOCR:
      return ocr_handler_ &&
             ocr_handler_->IsProcessRunningForTesting();  // IN-TEST
  }
}

}  // namespace screen_ai

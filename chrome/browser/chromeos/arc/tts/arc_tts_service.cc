// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tts/arc_tts_service.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/tts_controller.h"

namespace arc {
namespace {

// Singleton factory for ArcTtsService.
class ArcTtsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcTtsService,
          ArcTtsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcTtsServiceFactory";

  static ArcTtsServiceFactory* GetInstance() {
    return base::Singleton<ArcTtsServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcTtsServiceFactory>;
  ArcTtsServiceFactory() = default;
  ~ArcTtsServiceFactory() override = default;
};

}  // namespace

// static
ArcTtsService* ArcTtsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcTtsServiceFactory::GetForBrowserContext(context);
}

// static
ArcTtsService* ArcTtsService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcTtsServiceFactory::GetForBrowserContextForTesting(context);
}

ArcTtsService::ArcTtsService(content::BrowserContext* context,
                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), tts_controller_(nullptr) {
  arc_bridge_service_->tts()->SetHost(this);
}

ArcTtsService::~ArcTtsService() {
  arc_bridge_service_->tts()->SetHost(nullptr);
}

void ArcTtsService::OnTtsEvent(uint32_t id,
                               mojom::TtsEventType event_type,
                               uint32_t char_index,
                               const std::string& error_msg) {
  if (!tts_controller_) {
    // GetInstance() returns a base::Singleton<> object which always outlives
    // |this| object.
    tts_controller_ = content::TtsController::GetInstance();
    if (!tts_controller_) {
      LOG(WARNING) << "TtsController is not available.";
      return;
    }
  }

  content::TtsEventType chrome_event_type;
  switch (event_type) {
    case mojom::TtsEventType::START:
      chrome_event_type = content::TTS_EVENT_START;
      break;
    case mojom::TtsEventType::END:
      chrome_event_type = content::TTS_EVENT_END;
      break;
    case mojom::TtsEventType::INTERRUPTED:
      chrome_event_type = content::TTS_EVENT_INTERRUPTED;
      break;
    case mojom::TtsEventType::ERROR:
      chrome_event_type = content::TTS_EVENT_ERROR;
      break;
  }
  tts_controller_->OnTtsEvent(id, chrome_event_type, char_index, -1, error_msg);
}

}  // namespace arc

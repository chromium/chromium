// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tts/arc_tts_service.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/speech/tts_chromeos.h"
#include "content/public/browser/tts_controller.h"
#include "third_party/icu/source/common/unicode/uloc.h"

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

std::string CanonicalizeLocale(std::string locale) {
  UErrorCode error = U_ZERO_ERROR;

  // We only ever expect a maximum of a three-letter language code, a separator,
  // followed by a three-letter country code.
  static constexpr int kBufferSize = 8;
  std::string canonical_locale;
  int actual_size = uloc_canonicalize(
      locale.c_str(), base::WriteInto(&canonical_locale, kBufferSize),
      kBufferSize, &error);

  if (actual_size == 0 || error != U_ZERO_ERROR)
    return locale;

  canonical_locale.resize(actual_size);
  return canonical_locale;
}

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
                               uint32_t length,
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
    case mojom::TtsEventType::WORD:
      chrome_event_type = content::TTS_EVENT_WORD;
      TtsPlatformImplChromeOs::GetInstance()->ReceivedWordEvent();
  }
  tts_controller_->OnTtsEvent(id, chrome_event_type, char_index, length,
                              error_msg);
}

void ArcTtsService::OnVoicesChanged(std::vector<mojom::TtsVoicePtr> voices) {
  std::vector<content::VoiceData> chrome_voices;
  for (const auto& voice : voices) {
    chrome_voices.emplace_back();
    content::VoiceData& chrome_voice = chrome_voices.back();
    chrome_voice.native = true;
    chrome_voice.native_voice_identifier = base::NumberToString(voice->id);
    chrome_voice.name = std::move(voice->name);

    // Normalizes using ICU; in particular, turns language codes from three to
    // two letter codes. Then, replaces _ with - (expected by Chrome's tts
    // controller).
    chrome_voice.lang = CanonicalizeLocale(std::move(voice->locale));
    for (size_t i = 0; i < chrome_voice.lang.size(); i++) {
      if (chrome_voice.lang[i] == '_')
        chrome_voice.lang[i] = '-';
    }

    chrome_voice.remote = voice->is_network_connection_required;
    chrome_voice.events.insert(content::TTS_EVENT_START);
    chrome_voice.events.insert(content::TTS_EVENT_END);
    chrome_voice.events.insert(content::TTS_EVENT_INTERRUPTED);
    chrome_voice.events.insert(content::TTS_EVENT_ERROR);
  }

  TtsPlatformImplChromeOs* impl = TtsPlatformImplChromeOs::GetInstance();
  DCHECK(impl);
  impl->SetVoices(std::move(chrome_voices));

  content::TtsController::GetInstance()->VoicesChanged();
}

// static
void ArcTtsService::EnsureFactoryBuilt() {
  ArcTtsServiceFactory::GetInstance();
}

}  // namespace arc

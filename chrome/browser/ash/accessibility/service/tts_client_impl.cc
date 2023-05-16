// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/tts_client_impl.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/tts_controller.h"
#include "services/accessibility/public/mojom/tts.mojom.h"

namespace ash {

namespace {

ax::mojom::TtsEventType ToMojo(content::TtsEventType event_type) {
  switch (event_type) {
    case content::TTS_EVENT_START:
      return ax::mojom::TtsEventType::kStart;
    case content::TTS_EVENT_END:
      return ax::mojom::TtsEventType::kEnd;
    case content::TTS_EVENT_WORD:
      return ax::mojom::TtsEventType::kWord;
    case content::TTS_EVENT_SENTENCE:
      return ax::mojom::TtsEventType::kSentence;
    case content::TTS_EVENT_MARKER:
      return ax::mojom::TtsEventType::kMarker;
    case content::TTS_EVENT_INTERRUPTED:
      return ax::mojom::TtsEventType::kInterrupted;
    case content::TTS_EVENT_CANCELLED:
      return ax::mojom::TtsEventType::kCancelled;
    case content::TTS_EVENT_ERROR:
      return ax::mojom::TtsEventType::kError;
    case content::TTS_EVENT_PAUSE:
      return ax::mojom::TtsEventType::kPause;
    case content::TTS_EVENT_RESUME:
      return ax::mojom::TtsEventType::kResume;
  }
}

}  // namespace

TtsClientImpl::TtsClientImpl(content::BrowserContext* profile)
    : profile_(profile) {
  CHECK(profile_);
}

TtsClientImpl::~TtsClientImpl() = default;

void TtsClientImpl::Bind(mojo::PendingReceiver<Tts> tts_receiver) {
  tts_receivers_.Add(this, std::move(tts_receiver));
}

void TtsClientImpl::GetVoices(GetVoicesCallback callback) {
  std::vector<content::VoiceData> voices;
  // TODO(b:277221897): Pass a fake GURL matching the extension URL so that
  // Select to Speak can get the enhanced network voices.
  content::TtsController::GetInstance()->GetVoices(profile_, GURL(""), &voices);
  std::vector<ax::mojom::TtsVoicePtr> results;
  for (auto& voice : voices) {
    auto result = ax::mojom::TtsVoice::New();
    result->voice_name = voice.name;
    result->lang = voice.lang;
    result->remote = voice.remote;
    result->engine_id = voice.engine_id;
    if (!voice.events.empty()) {
      result->event_types = std::vector<ax::mojom::TtsEventType>();
      for (auto type : voice.events) {
        result->event_types->emplace_back(ToMojo(type));
      }
    }
    results.emplace_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace ash

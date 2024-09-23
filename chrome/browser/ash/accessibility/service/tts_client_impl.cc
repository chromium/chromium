// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/tts_client_impl.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/tts_controller.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// The max utterance length allowed by the TTS extension API.
const int kMaxUtteranceLength = 32768;
// The minimum speech rate allowed by the TTS extension API.
const double kMinRate = 0.1;
// The maximum speech rate allowed by the TTS extension API.
const double kMaxRate = 10.0;
// The maximum speech pitch allowed by the TTS extension API.
const double kMaxPitch = 2.0;
// The maximum speech volume allowed by the TTS extension API.
const double kMaxVolume = 1.0;

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

// Self-owned, compare to TtsExtensionEventHandler.
class AtpTtsEventHandler : public content::UtteranceEventDelegate {
 public:
  // static creator deals with "new" so clients don't have to think about it.
  static AtpTtsEventHandler* Create() { return new AtpTtsEventHandler(); }
  ~AtpTtsEventHandler() override = default;
  AtpTtsEventHandler(const AtpTtsEventHandler&) = delete;
  AtpTtsEventHandler& operator=(const AtpTtsEventHandler&) = delete;

  // content::UtteranceEventDelegate:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override {
    auto mojom_event = ax::mojom::TtsEvent::New();
    mojom_event->type = ToMojo(event_type);
    mojom_event->char_index = char_index;
    mojom_event->length = length;
    mojom_event->is_final = utterance->IsFinished();
    if (event_type == content::TTS_EVENT_ERROR) {
      mojom_event->error_message = error_message;
    }
    utterance_client_->OnEvent(std::move(mojom_event));
    if (utterance->IsFinished()) {
      // Expected to self-destroy on call to TtsEvent, see
      // tts_utterance_impl.cc.
      delete this;
    }
  }
  mojo::PendingReceiver<ax::mojom::TtsUtteranceClient> PassReceiver() {
    return utterance_client_.BindNewPipeAndPassReceiver();
  }

 private:
  AtpTtsEventHandler() = default;
  mojo::Remote<ax::mojom::TtsUtteranceClient> utterance_client_;
};

}  // namespace

TtsClientImpl::TtsClientImpl(content::BrowserContext* profile)
    : profile_(profile) {
  CHECK(profile_);
}

TtsClientImpl::~TtsClientImpl() = default;

void TtsClientImpl::Bind(mojo::PendingReceiver<Tts> tts_receiver) {
  tts_receivers_.Add(this, std::move(tts_receiver));
}

void TtsClientImpl::Speak(const std::string& utterance,
                          ax::mojom::TtsOptionsPtr options,
                          SpeakCallback callback) {
  auto result = ax::mojom::TtsSpeakResult::New();
  if (utterance.size() > kMaxUtteranceLength) {
    result->error = ax::mojom::TtsError::kErrorUtteranceTooLong;
    std::move(callback).Run(std::move(result));
    return;
  }

  // Check for errors in options.
  // TODO(crbug.com/41278287): Centralize the struct validation.
  if (options->rate < kMinRate || options->rate > kMaxRate) {
    result->error = ax::mojom::TtsError::kErrorInvalidRate;
    std::move(callback).Run(std::move(result));
    return;
  }
  if (options->pitch < 0.0 || options->pitch > kMaxPitch) {
    result->error = ax::mojom::TtsError::kErrorInvalidPitch;
    std::move(callback).Run(std::move(result));
    return;
  }
  if (options->volume < 0.0 || options->volume > kMaxVolume) {
    result->error = ax::mojom::TtsError::kErrorInvalidVolume;
    std::move(callback).Run(std::move(result));
    return;
  }

  // Only make the utterance once we know we aren't going to return early.
  std::unique_ptr<content::TtsUtterance> tts_utterance =
      content::TtsUtterance::Create(profile_);
  tts_utterance->SetText(utterance);
  // TODO(b:277221897): Pass a fake GURL matching the ash extension URL.
  // This will support both UMA and using enhanced network voices in ATP
  // select-to-speak.
  tts_utterance->SetSrcUrl(GURL(""));

  tts_utterance->SetContinuousParameters(options->rate, options->pitch,
                                         options->volume);
  tts_utterance->SetShouldClearQueue(!options->enqueue);
  if (options->lang) {
    std::string lang = options->lang.value();
    if (!lang.empty() && !l10n_util::IsValidLocaleSyntax(lang)) {
      result->error = ax::mojom::TtsError::kErrorInvalidLang;
      std::move(callback).Run(std::move(result));
      return;
    }
    tts_utterance->SetLang(options->lang.value());
  }
  if (options->voice_name) {
    tts_utterance->SetVoiceName(options->voice_name.value());
  }
  if (options->engine_id) {
    tts_utterance->SetEngineId(options->engine_id.value());
  }
  if (options->on_event) {
    auto* atpTtsEventHandler = AtpTtsEventHandler::Create();
    result->utterance_client = atpTtsEventHandler->PassReceiver();
    tts_utterance->SetEventDelegate(atpTtsEventHandler);
  }
  // Note: we don't need desired/required event types because they aren't
  // passed by ChromeVox or STS. We don't need an options_dict, it's redundant,
  // and we don't need a src_id because each ATP utterance has its own callback.

  // Send the callback back to ATP with the utterance client.
  result->error = ax::mojom::TtsError::kNoError;
  std::move(callback).Run(std::move(result));

  // Start speech.
  content::TtsController* controller = content::TtsController::GetInstance();
  controller->SpeakOrEnqueue(std::move(tts_utterance));
}

void TtsClientImpl::Stop() {
  content::TtsController* controller = content::TtsController::GetInstance();
  // TODO(b:277221897): Pass a fake GURL matching the ash extension URL so that
  // extensions cannot clobber other speech.
  controller->Stop(GURL(""));
}

void TtsClientImpl::Pause() {
  content::TtsController::GetInstance()->Pause();
}

void TtsClientImpl::Resume() {
  content::TtsController::GetInstance()->Resume();
}

void TtsClientImpl::IsSpeaking(IsSpeakingCallback callback) {
  std::move(callback).Run(content::TtsController::GetInstance()->IsSpeaking());
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

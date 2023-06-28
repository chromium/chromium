// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace captions {

// static
void LiveCaptionSpeechRecognitionHost::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver) {
  CHECK(frame_host);
  // The object is bound to the lifetime of |host| and the mojo
  // connection. See DocumentService for details.
  new LiveCaptionSpeechRecognitionHost(*frame_host, std::move(receiver));
}

LiveCaptionSpeechRecognitionHost::LiveCaptionSpeechRecognitionHost(
    content::RenderFrameHost& frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver)
    : DocumentService<media::mojom::SpeechRecognitionRecognizerClient>(
          frame_host,
          std::move(receiver)) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  Observe(web_contents);
  prefs_ = Profile::FromBrowserContext(GetWebContents()->GetBrowserContext())
               ->GetPrefs();
  context_ = CaptionBubbleContextBrowser::Create(web_contents);

  source_language_ = prefs_->GetString(prefs::kLiveCaptionLanguageCode);
}

LiveCaptionSpeechRecognitionHost::~LiveCaptionSpeechRecognitionHost() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(context_.get());
  if (base::FeatureList::IsEnabled(media::kLiveTranslate) &&
      characters_translated_ > 0) {
    base::UmaHistogramCounts10M(
        "Accessibility.LiveTranslate.CharactersTranslated",
        characters_translated_);
  }
}

void LiveCaptionSpeechRecognitionHost::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (!live_caption_controller) {
    std::move(reply).Run(false);
    return;
  }

  if (base::FeatureList::IsEnabled(media::kLiveTranslate) &&
      prefs_->GetBoolean(prefs::kLiveTranslateEnabled) &&
      l10n_util::GetLanguage(
          prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode)) !=
          l10n_util::GetLanguage(source_language_)) {
    characters_translated_ += result.transcription.size();
    GetLiveTranslateController()->GetTranslation(
        result, source_language_,
        prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode),
        base::BindOnce(&LiveCaptionSpeechRecognitionHost::OnTranslationCallback,
                       weak_factory_.GetWeakPtr()));
    std::move(reply).Run(!stop_transcriptions_);
  } else {
    std::move(reply).Run(
        live_caption_controller->DispatchTranscription(context_.get(), result));
  }
}

void LiveCaptionSpeechRecognitionHost::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (!live_caption_controller)
    return;

  if (event->asr_switch_result ==
      media::mojom::AsrSwitchResult::kSwitchSucceeded) {
    source_language_ = event->language;
  }

  live_caption_controller->OnLanguageIdentificationEvent(context_.get(),
                                                         std::move(event));
}

void LiveCaptionSpeechRecognitionHost::OnSpeechRecognitionError() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnError(
        context_.get(), CaptionBubbleErrorType::kGeneric,
        base::RepeatingClosure(),
        base::BindRepeating(
            [](CaptionBubbleErrorType error_type, bool checked) {}));
}

void LiveCaptionSpeechRecognitionHost::OnSpeechRecognitionStopped() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(context_.get());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
void LiveCaptionSpeechRecognitionHost::MediaEffectivelyFullscreenChanged(
    bool is_fullscreen) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnToggleFullscreen(context_.get());
}
#endif

void LiveCaptionSpeechRecognitionHost::OnTranslationCallback(
    media::SpeechRecognitionResult result) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  stop_transcriptions_ =
      !live_caption_controller->DispatchTranscription(context_.get(), result);
}

content::WebContents* LiveCaptionSpeechRecognitionHost::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(&render_frame_host());
}

LiveCaptionController*
LiveCaptionSpeechRecognitionHost::GetLiveCaptionController() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return nullptr;
  return LiveCaptionControllerFactory::GetForProfile(profile);
}

LiveTranslateController*
LiveCaptionSpeechRecognitionHost::GetLiveTranslateController() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return LiveTranslateControllerFactory::GetForProfile(profile);
}

}  // namespace captions

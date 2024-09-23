// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/greedy_text_stabilizer.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/translation_util.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
static constexpr int kMinTokenFrequency = 1;
static constexpr int kWaitKValue = 1;

// The number of consecutive highly confident language identification events
// required to trigger an automatic download of the missing language pack.
static constexpr int kLanguageIdentificationEventCountThreshold = 3;

std::string RemoveLastKWords(const std::string& input) {
  int words_to_remove = kWaitKValue;

  if (words_to_remove == 0) {
    return input;
  }

  size_t length = input.length();
  size_t last_space_pos = 0;

  while (words_to_remove > 0 && length > 0) {
    length--;
    if (std::isspace(input[length])) {
      words_to_remove--;
      last_space_pos = length;
    }
  }

  if (words_to_remove == 0) {
    return input.substr(0, last_space_pos);
  } else {
    return std::string();
  }
}

// Returns a boolean indicating whether the language is both enabled and not
// already installed.
bool IsLanguageInstallable(const std::string& language_code) {
  for (const auto& language : g_browser_process->local_state()->GetList(
           prefs::kSodaRegisteredLanguagePacks)) {
    if (language.GetString() == language_code) {
      return false;
    }
  }

  return base::Contains(
      speech::SodaInstaller::GetInstance()->GetLiveCaptionEnabledLanguages(),
      language_code);
}

}  // namespace

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

  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionUseGreedyTextStabilizer)) {
    greedy_text_stabilizer_ =
        std::make_unique<captions::GreedyTextStabilizer>(kMinTokenFrequency);
  }
}

LiveCaptionSpeechRecognitionHost::~LiveCaptionSpeechRecognitionHost() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(context_.get());
  if (media::IsLiveTranslateEnabled() && characters_translated_ > 0) {
    base::UmaHistogramCounts10M(
        "Accessibility.LiveTranslate.CharactersTranslated",
        characters_translated_);

    if (base::FeatureList::IsEnabled(media::kLiveCaptionLogFlickerRate)) {
      // Log the average number of characters omitted from the translation by
      // the text stabilization policy per partial recognition result.
      double lag_rate =
          (partial_result_count_ > 0)
              ? translation_characters_erased_ / partial_result_count_
              : 0;
      LOG(WARNING) << "Live caption average lag rate:" << lag_rate
                   << ". (not a warning)";
    }
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

  std::string target_language =
      prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode);
  if (media::IsLiveTranslateEnabled() &&
      prefs_->GetBoolean(prefs::kLiveTranslateEnabled) &&
      l10n_util::GetLanguage(target_language) !=
          l10n_util::GetLanguage(source_language_)) {
    auto cache_result = translation_cache_.FindCachedTranslationOrRemaining(
        result.transcription, source_language_, target_language);

    std::string cached_translation = cache_result.second;
    std::string string_to_translate = cache_result.first;

    if (!string_to_translate.empty()) {
      characters_translated_ += string_to_translate.size();
      GetLiveTranslateController()->GetTranslation(
          string_to_translate, source_language_, target_language,
          base::BindOnce(
              &LiveCaptionSpeechRecognitionHost::OnTranslationCallback,
              weak_factory_.GetWeakPtr(), cached_translation,
              string_to_translate, source_language_, target_language,
              result.is_final));
      std::move(reply).Run(!stop_transcriptions_);
    } else {
      // Dispatch the transcription immediately if the entire transcription was
      // cached.
      std::move(reply).Run(live_caption_controller->DispatchTranscription(
          context_.get(),
          media::SpeechRecognitionResult(
              GetTextForDispatch(cached_translation, result.is_final),
              result.is_final)));
    }
  } else {
    std::move(reply).Run(live_caption_controller->DispatchTranscription(
        context_.get(),
        media::SpeechRecognitionResult(
            GetTextForDispatch(result.transcription, result.is_final),
            result.is_final)));
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

  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionAutomaticLanguageDownload)) {
    if (auto_detected_language_ != event->language) {
      language_identification_event_count_ = 0;
      auto_detected_language_ = event->language;
    }

    if (event->confidence_level ==
        media::mojom::ConfidenceLevel::kHighlyConfident) {
      language_identification_event_count_++;
    } else {
      language_identification_event_count_ = 0;
    }

    if (language_identification_event_count_ ==
        kLanguageIdentificationEventCountThreshold) {
      std::optional<speech::SodaLanguagePackComponentConfig> language_config =
          speech::GetLanguageComponentConfigMatchingLanguageSubtag(
              event->language);

      if (language_config.has_value() &&
          IsLanguageInstallable(language_config.value().language_name)) {
        // InstallLanguage will only install languages that are not already
        // installed.
        speech::SodaInstaller::GetInstance()->InstallLanguage(
            language_config.value().language_name,
            g_browser_process->local_state());
      }
    }
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
    const std::string& cached_translation,
    const std::string& original_transcription,
    const std::string& source_language,
    const std::string& target_language,
    bool is_final,
    const std::string& result) {
  std::string formatted_result = result;
  // Don't cache the translation if the source language is an ideographic
  // language but the target language is not to avoid translate
  // sentence by sentence because the Cloud Translation API does not properly
  // translate ideographic punctuation marks.
  if (!IsIdeographicLocale(source_language) ||
      IsIdeographicLocale(target_language)) {
    if (is_final) {
      translation_cache_.Clear();
    } else {
      translation_cache_.InsertIntoCache(original_transcription, result,
                                         source_language, target_language);
    }
  } else {
    // Append a space after final results when translating from an ideographic
    // to non-ideographic locale. The Speech On-Device API (SODA) automatically
    // prepends a space to recognition events after a final event, but only for
    // non-ideographic locales.
    // TODO(crbug.com/40261536): Consider moving this to the
    // LiveTranslateController.
    if (is_final) {
      formatted_result += " ";
    }
  }

  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  auto text = base::StrCat({cached_translation, formatted_result});

  stop_transcriptions_ = !live_caption_controller->DispatchTranscription(
      context_.get(), media::SpeechRecognitionResult(
                          GetTextForDispatch(text, is_final), is_final));
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

std::string LiveCaptionSpeechRecognitionHost::GetTextForDispatch(
    const std::string& input_text,
    bool is_final) {
  std::string text = input_text;
  if (base::FeatureList::IsEnabled(media::kLiveCaptionUseWaitK) && !is_final) {
    text = RemoveLastKWords(text);
  }

  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionUseGreedyTextStabilizer)) {
    text = greedy_text_stabilizer_->UpdateText(text, is_final);
  }

  if (media::IsLiveTranslateEnabled()) {
    translation_characters_erased_ += input_text.length() - text.length();
    partial_result_count_++;
  }

  return text;
}
}  // namespace captions

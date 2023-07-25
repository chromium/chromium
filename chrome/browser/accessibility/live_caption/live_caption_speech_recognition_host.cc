// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kSpaceChar = ' ';

// Split the transcription into sentences. Spaces are included in the preceding
// sentence.
std::vector<std::string> SplitSentences(const std::string& text,
                                        const std::string& locale) {
  std::vector<std::string> sentences;
  UErrorCode status = U_ZERO_ERROR;

  // Use icu::BreakIterator instead of base::i18n::BreakIterator to avoid flakey
  // mid-string sentence breaks.
  icu::BreakIterator* iter =
      icu::BreakIterator::createSentenceInstance(locale.c_str(), status);

  DCHECK(U_SUCCESS(status))
      << "ICU could not open a break iterator: " << u_errorName(status) << " ("
      << status << ")";

  // Set the text to be analyzed.
  icu::UnicodeString unicode_text = icu::UnicodeString::fromUTF8(text);
  iter->setText(unicode_text);

  // Iterate over the sentences.
  int32_t start = iter->first();
  int32_t end = iter->next();
  while (end != icu::BreakIterator::DONE) {
    icu::UnicodeString sentence;
    unicode_text.extractBetween(start, end, sentence);
    std::string sentence_string;
    sentence.toUTF8String(sentence_string);
    sentences.emplace_back(sentence_string);
    start = end;
    end = iter->next();
  }

  delete iter;

  return sentences;
}

bool ContainsTrailingSpace(const std::string& str) {
  return !str.empty() && base::IsAsciiWhitespace(str.back());
}

std::string RemoveTrailingSpace(const std::string& str) {
  if (ContainsTrailingSpace(str)) {
    return str.substr(0, str.length() - 1);
  }

  return str;
}

std::string RemovePunctuationToLower(std::string str) {
  re2::RE2::GlobalReplace(&str, "[[:punct:]]", "");

  return base::ToLowerASCII(str);
}

std::string GetTranslationCacheKey(const std::string& source_language,
                                   const std::string& target_language,
                                   const std::string& transcription) {
  return base::StrCat({source_language, target_language, "|",
                       RemovePunctuationToLower(transcription)});
}

bool IsIdeographicLocale(const std::string& locale) {
  // Retrieve the script codes used by the given language from ICU. When the
  // given language consists of two or more scripts, we just use the first
  // script. The size of returned script codes is always < 8. Therefore, we use
  // an array of size 8 so we can include all script codes without insufficient
  // buffer errors.
  UErrorCode error = U_ZERO_ERROR;
  UScriptCode script_code[8];
  int scripts = uscript_getCode(locale.c_str(), script_code,
                                std::size(script_code), &error);

  return U_SUCCESS(error) && scripts >= 1 &&
         (script_code[0] == USCRIPT_HAN || script_code[0] == USCRIPT_HIRAGANA ||
          script_code[0] == USCRIPT_YI || script_code[0] == USCRIPT_KATAKANA);
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

  std::string target_language =
      prefs_->GetString(prefs::kLiveTranslateTargetLanguageCode);
  if (base::FeatureList::IsEnabled(media::kLiveTranslate) &&
      prefs_->GetBoolean(prefs::kLiveTranslateEnabled) &&
      l10n_util::GetLanguage(target_language) !=
          l10n_util::GetLanguage(source_language_)) {
    std::vector<std::string> sentences =
        SplitSentences(result.transcription, source_language_);

    std::string cached_translation;
    std::string string_to_translate;
    bool cached_translation_found = true;
    for (std::string& sentence : sentences) {
      if (cached_translation_found) {
        bool sentence_contains_trailing_space = ContainsTrailingSpace(sentence);
        auto translation_cache_key = GetTranslationCacheKey(
            source_language_, target_language,
            sentence_contains_trailing_space ? RemoveTrailingSpace(sentence)
                                             : sentence);
        auto iter = translation_cache_.find(translation_cache_key);
        if (iter != translation_cache_.end()) {
          cached_translation += iter->second;
          if (sentence_contains_trailing_space) {
            cached_translation += kSpaceChar;
          }

          continue;
        }
        cached_translation_found = false;
      }

      string_to_translate = base::StrCat({string_to_translate, sentence});
    }

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
          media::SpeechRecognitionResult(cached_translation, result.is_final)));
    }
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
    auto original_sentences =
        SplitSentences(original_transcription, source_language);
    auto translated_sentences = SplitSentences(result, target_language);
    if (is_final) {
      translation_cache_.clear();
    } else {
      if (original_sentences.size() > 1 &&
          original_sentences.size() == translated_sentences.size()) {
        for (size_t i = 0; i < original_sentences.size() - 1; i++) {
          // Sentences are always cached without the trailing space.
          std::string sentence = RemoveTrailingSpace(original_sentences[i]);
          translation_cache_.insert(
              {GetTranslationCacheKey(source_language, target_language,
                                      sentence),
               RemoveTrailingSpace(translated_sentences[i])});
        }
      }
    }
  } else {
    // Append a space after final results when translating from an ideographic
    // to non-ideographic locale. The Speech On-Device API (SODA) automatically
    // prepends a space to recognition events after a final event, but only for
    // non-ideographic locales.
    // TODO(crbug.com/1426899): Consider moving this to the LiveTranslateController.
    if (is_final) {
      formatted_result += " ";
    }
  }

  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  stop_transcriptions_ = !live_caption_controller->DispatchTranscription(
      context_.get(),
      media::SpeechRecognitionResult(
          base::StrCat({cached_translation, formatted_result}), is_final));
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

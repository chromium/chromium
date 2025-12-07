// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translator.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"
#include "url/origin.h"

namespace on_device_translation {

namespace {

bool IsTranslatableCharacter(char character) {
  return !base::IsAsciiWhitespace(character) &&
         !base::IsAsciiControl(character);
}

bool ContainsTranslatableContent(const std::string& input) {
  return std::any_of(input.begin(), input.end(), IsTranslatableCharacter);
}

}  // namespace

Translator::Translator(
    base::WeakPtr<content::BrowserContext> browser_context,
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingRemote<on_device_translation::mojom::Translator> remote)
    : browser_context_(browser_context),
      source_lang_(source_lang),
      target_lang_(target_lang),
      translator_remote_(std::move(remote)) {}

Translator::~Translator() = default;

bool Translator::VerifyPrerequisites(
    const std::string& input,
    mojo::Remote<blink::mojom::ModelStreamingResponder>& responder) {
  if (!Profile::FromBrowserContext(browser_context_.get())
           ->GetPrefs()
           ->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return false;
  }

  RecordTranslationAPICallForLanguagePair("Translate", source_lang_,
                                          target_lang_);
  RecordTranslationCharacterCount(source_lang_, target_lang_, input.size());

  // https://github.com/webmachinelearning/translation-api/pull/38: "If |input|
  // is the empty string, or otherwise consists of no translatable content
  // (e.g., only contains whitespace, or control characters), then the resulting
  // translation should be |input|. In such cases, |sourceLanguage| and
  // |targetLanguage| should be ignored."
  if (!ContainsTranslatableContent(input)) {
    responder->OnStreaming(input);
    responder->OnCompletion(/*context_info=*/nullptr);
    return false;
  }
  return true;
}

void Translator::Translate(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  CHECK(browser_context_);
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));

  if (!VerifyPrerequisites(input, responder)) {
    return;
  }

  if (translator_remote_.is_connected()) {
    translator_remote_->Translate(
        input,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(
                [](mojo::Remote<blink::mojom::ModelStreamingResponder>
                       responder,
                   const std::optional<std::string>& output) {
                  if (!output) {
                    responder->OnError(
                        blink::mojom::ModelStreamingResponseStatus::
                            kErrorGenericFailure,
                        /*quota_error_info=*/nullptr);
                    return;
                  }
                  responder->OnStreaming(output.value());
                  responder->OnCompletion(/*context_info=*/nullptr);
                },
                std::move(responder)),
            std::nullopt));
  } else {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
  }
}

void Translator::SplitSentencesCallback(
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder,
    const std::vector<std::string>& sentences) {
  if (sentences.empty()) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return;
  }
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(responder));

  pending_translations_[responder_id] = sentences.size();
  const int total_translations = sentences.size();
  for (const auto& sentence : sentences) {
    translator_remote_->Translate(
        sentence, base::BindOnce(&Translator::TranslateStreamingCallback,
                                 weak_ptr_factory_.GetWeakPtr(), responder_id,
                                 total_translations));
  }
}
void Translator::TranslateStreamingCallback(
    mojo::RemoteSetElementId responder_id,
    int total_translations,
    const std::optional<std::string>& output) {
  auto it = pending_translations_.find(responder_id);

  blink::mojom::ModelStreamingResponder* responder_ptr =
      responder_set_.Get(responder_id);

  // This should only happen after the responder disconnected.
  if (it == pending_translations_.end()) {
    CHECK(!responder_ptr);
    return;
  }

  // This indicates that the responder disconnected.
  if (!responder_ptr) {
    pending_translations_.erase(it);
    return;
  }

  if (!output) {
    responder_ptr->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    responder_set_.Remove(responder_id);
    pending_translations_.erase(it);
    return;
  }

  // Each Translate() call strips leading and trailing whitespace.
  // If this is the first sentence we are streaming, do not prepend a space.
  // Otherwise, prepend a space to the chunk.
  const int pending_translations = it->second;
  if (pending_translations == total_translations) {
    responder_ptr->OnStreaming(output.value());
  } else {
    responder_ptr->OnStreaming(base::StrCat({" ", output.value()}));
  }

  if (--it->second == 0) {
    responder_ptr->OnCompletion(/*context_info=*/nullptr);
    responder_set_.Remove(responder_id);
    pending_translations_.erase(it);
  }
}

void Translator::TranslateStreaming(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  CHECK(browser_context_);
  if (!base::FeatureList::IsEnabled(kTranslateStreamingBySentence)) {
    Translate(input, std::move(pending_responder));
    return;
  }
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));

  if (!VerifyPrerequisites(input, responder)) {
    return;
  }

  if (!translator_remote_.is_connected()) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return;
  }

  translator_remote_->SplitSentences(
      input,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&Translator::SplitSentencesCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(responder)),
          std::vector<std::string>()));
}

}  // namespace on_device_translation

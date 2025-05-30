// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translator.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
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

void Translator::Translate(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  CHECK(browser_context_);
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));
  if (!Profile::FromBrowserContext(browser_context_.get())
           ->GetPrefs()
           ->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return;
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
                  responder->OnStreaming(*output);
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

}  // namespace on_device_translation

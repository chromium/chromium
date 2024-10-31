// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translator.h"

#include "base/functional/bind.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

namespace on_device_translation {

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

void Translator::Translate(const std::string& input,
                           TranslateCallback callback) {
  CHECK(browser_context_);
  if (!Profile::FromBrowserContext(browser_context_.get())
           ->GetPrefs()
           ->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  RecordTranslationAPICallForLanguagePair("Translate", source_lang_,
                                          target_lang_);
  RecordTranslationCharacterCount(source_lang_, target_lang_, input.size());
  if (translator_remote_.is_connected()) {
    translator_remote_->Translate(
        input, mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                           std::nullopt));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

}  // namespace on_device_translation

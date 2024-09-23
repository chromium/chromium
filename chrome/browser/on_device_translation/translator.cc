// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translator.h"

#include "base/functional/bind.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

Translator::Translator(const std::string& source_lang,
                       const std::string& target_lang,
                       base::OnceCallback<void(bool)> callback) {
  OnDeviceTranslationServiceController::GetInstance()->CreateTranslator(
      source_lang, target_lang, translator_remote_.BindNewPipeAndPassReceiver(),
      std::move(callback));
}

Translator::~Translator() = default;

void Translator::Translate(const std::string& input,
                           TranslateCallback callback) {
  if (translator_remote_.is_connected()) {
    translator_remote_->Translate(
        input, base::BindOnce(
                   [](TranslateCallback callback, const std::string& output) {
                     std::move(callback).Run(output);
                   },
                   std::move(callback)));
  } else {
    std::move(callback).Run(nullptr);
  }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATOR_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATOR_H_

#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

// The browser-side implementation of `blink::mojom::Translator`, which
// exposes the `Translate()` method to do translation.
class Translator : public blink::mojom::Translator {
 public:
  Translator(const std::string& source_lang,
             const std::string& target_lang,
             base::OnceCallback<void(bool)> callback);

  Translator(const Translator&) = delete;
  Translator& operator=(const Translator&) = delete;

  ~Translator() override;

  // `blink::mojom::Translator` implementation.
  void Translate(const std::string& input, TranslateCallback callback) override;

 private:
  mojo::Remote<on_device_translation::mojom::Translator> translator_remote_;
};

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATOR_H_

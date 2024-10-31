// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATOR_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATOR_H_

#include "base/memory/weak_ptr.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace on_device_translation {

// The browser-side implementation of `blink::mojom::Translator`, which
// exposes the `Translate()` method to do translation.
class Translator : public blink::mojom::Translator {
 public:
  Translator(
      base::WeakPtr<content::BrowserContext> browser_context,
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingRemote<on_device_translation::mojom::Translator> remote);

  Translator(const Translator&) = delete;
  Translator& operator=(const Translator&) = delete;

  ~Translator() override;

  // `blink::mojom::Translator` implementation.
  void Translate(const std::string& input, TranslateCallback callback) override;

 private:
  base::WeakPtr<content::BrowserContext> browser_context_;
  const std::string source_lang_;
  const std::string target_lang_;
  mojo::Remote<on_device_translation::mojom::Translator> translator_remote_;
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATOR_H_

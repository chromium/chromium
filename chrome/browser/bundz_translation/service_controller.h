// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUNDZ_TRANSLATION_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_BUNDZ_TRANSLATION_SERVICE_CONTROLLER_H_

#include "base/no_destructor.h"
#include "chrome/services/bundz_translation/public/mojom/bundz_translation_service.mojom.h"
#include "chrome/services/bundz_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class is the controller that launches the bundz translation service and
// delegates the functionalities.
class BundzTranslationServiceController {
 public:
  BundzTranslationServiceController(const BundzTranslationServiceController&) =
      delete;
  BundzTranslationServiceController& operator=(
      const BundzTranslationServiceController&) = delete;

  static BundzTranslationServiceController* GetInstance();

  // Creates a translator class that implements
  // `bundz_translation::mojom::Translator`, and bind it with the `receiver`.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<bundz_translation::mojom::Translator> receiver,
      base::OnceCallback<void(bool)> callback);

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    base::OnceCallback<void(bool)> callback);

 private:
  friend base::NoDestructor<BundzTranslationServiceController>;

  BundzTranslationServiceController();
  ~BundzTranslationServiceController();

  void LaunchService();

  mojo::Remote<bundz_translation::mojom::BundzTranslationService>
      service_remote_;
};

#endif  // CHROME_BROWSER_BUNDZ_TRANSLATION_SERVICE_CONTROLLER_H_

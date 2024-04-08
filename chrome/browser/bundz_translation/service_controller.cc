// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bundz_translation/service_controller.h"

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "chrome/services/bundz_translation/public/mojom/bundz_translation_service.mojom.h"
#include "chrome/services/bundz_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"

const char kBundzTranslationServiceDisplayName[] = "Bundz Translation Service";

BundzTranslationServiceController::BundzTranslationServiceController() {
  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();
  content::ServiceProcessHost::Launch<
      bundz_translation::mojom::BundzTranslationService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName(kBundzTranslationServiceDisplayName)
          .Pass());
}

BundzTranslationServiceController::~BundzTranslationServiceController() =
    default;

void BundzTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<bundz_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> callback) {
  service_remote_->CreateTranslator(source_lang, target_lang,
                                    std::move(receiver), std::move(callback));
}

void BundzTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(bool)> callback) {
  service_remote_->CanTranslate(source_lang, target_lang, std::move(callback));
}

// static
BundzTranslationServiceController*
BundzTranslationServiceController::GetInstance() {
  static base::NoDestructor<BundzTranslationServiceController> instance;
  return instance.get();
}

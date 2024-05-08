// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/service_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "chrome/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "chrome/services/on_device_translation/public/mojom/translator.mojom.h"
#include "content/public/browser/service_process_host.h"

const char kOnDeviceTranslationServiceDisplayName[] =
    "On Device Translation Service";

OnDeviceTranslationServiceController::OnDeviceTranslationServiceController() {
  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();
  content::ServiceProcessHost::Launch<
      on_device_translation::mojom::OnDeviceTranslationService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName(kOnDeviceTranslationServiceDisplayName)
          .Pass());
}

OnDeviceTranslationServiceController::~OnDeviceTranslationServiceController() =
    default;

void OnDeviceTranslationServiceController::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> callback) {
  service_remote_->CreateTranslator(source_lang, target_lang,
                                    std::move(receiver), std::move(callback));
}

void OnDeviceTranslationServiceController::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(bool)> callback) {
  service_remote_->CanTranslate(source_lang, target_lang, std::move(callback));
}

// static
OnDeviceTranslationServiceController*
OnDeviceTranslationServiceController::GetInstance() {
  static base::NoDestructor<OnDeviceTranslationServiceController> instance;
  return instance.get();
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/translator.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"

DOCUMENT_USER_DATA_KEY_IMPL(TranslationManagerImpl);

TranslationManagerImpl::TranslationManagerImpl(content::RenderFrameHost* rfh)
    : DocumentUserData<TranslationManagerImpl>(rfh) {
  browser_context_ = rfh->GetBrowserContext()->GetWeakPtr();
}

TranslationManagerImpl::~TranslationManagerImpl() = default;

// static
void TranslationManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
  TranslationManagerImpl* translation_manager =
      TranslationManagerImpl::GetOrCreateForCurrentDocument(render_frame_host);
  translation_manager->receiver_.Bind(std::move(receiver));
}

void TranslationManagerImpl::CanCreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    CanCreateTranslatorCallback callback) {
  OnDeviceTranslationServiceController::GetInstance()->CanTranslate(
      source_lang, target_lang, std::move(callback));
}

void TranslationManagerImpl::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<blink::mojom::Translator> receiver,
    CreateTranslatorCallback callback) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Translator>(source_lang, target_lang,
                                   std::move(callback)),
      std::move(receiver));
}

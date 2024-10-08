// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

// The browser-side implementation of `blink::mojom::TranslationManager`, it
// should be destroyed together with the associated RFH or when the RFH is used
// for a cross-document navigation.
class TranslationManagerImpl
    : public content::DocumentUserData<TranslationManagerImpl>,
      public blink::mojom::TranslationManager {
 public:
  TranslationManagerImpl(const TranslationManagerImpl&) = delete;
  TranslationManagerImpl& operator=(const TranslationManagerImpl&) = delete;

  ~TranslationManagerImpl() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::TranslationManager> receiver);

 private:
  friend class DocumentUserData<TranslationManagerImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit TranslationManagerImpl(content::RenderFrameHost* rfh);

  // `blink::mojom::TranslationManager` implementation.
  void CanCreateTranslator(const std::string& source_lang,
                           const std::string& target_lang,
                           CanCreateTranslatorCallback callback) override;

  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<blink::mojom::Translator> receiver,
      CreateTranslatorCallback callback) override;

#if !BUILDFLAG(IS_ANDROID)
  bool PassAcceptLanguagesCheck(const std::string& source_lang,
                                const std::string& target_lang);
#endif  // !BUILDFLAG(IS_ANDROID)

  base::WeakPtr<content::BrowserContext> browser_context_;
  mojo::Receiver<blink::mojom::TranslationManager> receiver_{this};
};

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

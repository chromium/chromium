// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"
#include "url/origin.h"

namespace on_device_translation {

class OnDeviceTranslationServiceController;

// The browser-side implementation of `blink::mojom::TranslationManager`, it
// is owned by a SupportsUserData (DocumentAssociatedData for frames,
// DedicatedWorkerHost, SharedWorkerHost and ServiceWorkerHost).
class TranslationManagerImpl : public base::SupportsUserData::Data,
                               public blink::mojom::TranslationManager {
 public:
  TranslationManagerImpl(base::PassKey<TranslationManagerImpl>,
                         content::BrowserContext* browser_context,
                         const url::Origin& origin);
  TranslationManagerImpl(const TranslationManagerImpl&) = delete;
  TranslationManagerImpl& operator=(const TranslationManagerImpl&) = delete;

  ~TranslationManagerImpl() override;

  static void Bind(
      content::BrowserContext* browser_context,
      base::SupportsUserData* context_user_data,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::TranslationManager> receiver);

 private:
  friend class TranslationManagerImplTest;

  static TranslationManagerImpl* GetOrCreate(
      content::BrowserContext* browser_context,
      base::SupportsUserData* context_user_data,
      const url::Origin& origin);

  // `blink::mojom::TranslationManager` implementation.
  void CanCreateTranslator(blink::mojom::TranslatorLanguageCodePtr source_lang,
                           blink::mojom::TranslatorLanguageCodePtr target_lang,
                           CanCreateTranslatorCallback callback) override;
  void CreateTranslator(
      mojo::PendingRemote<
          blink::mojom::TranslationManagerCreateTranslatorClient> client,
      blink::mojom::TranslatorCreateOptionsPtr options) override;
  void GetTranslatorAvailabilityInfo(
      GetTranslatorAvailabilityInfoCallback callback) override;

  static bool PassAcceptLanguagesCheck(const std::string& accept_languages_str,
                                       const std::string& source_lang,
                                       const std::string& target_lang);

  OnDeviceTranslationServiceController& GetServiceController();

  const base::WeakPtr<content::BrowserContext> browser_context_;
  const url::Origin origin_;
  scoped_refptr<OnDeviceTranslationServiceController> service_controller_;
  mojo::UniqueReceiverSet<blink::mojom::Translator> translators_;
  mojo::ReceiverSet<blink::mojom::TranslationManager> receiver_set_;
  base::WeakPtrFactory<TranslationManagerImpl> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

#include "base/auto_reset.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_model_download_progress_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"
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

  // Sets an instance of `TranslationManagerImpl` for testing.
  static base::AutoReset<TranslationManagerImpl*> SetForTesting(
      TranslationManagerImpl* manager);

  static void Bind(
      content::BrowserContext* browser_context,
      base::SupportsUserData* context_user_data,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::TranslationManager> receiver);

 protected:
  TranslationManagerImpl(content::BrowserContext* browser_context,
                         const url::Origin& origin);

  content::BrowserContext* browser_context() { return browser_context_.get(); }

 private:
  friend class TranslationManagerImplTest;

  static TranslationManagerImpl* GetOrCreate(
      content::BrowserContext* browser_context,
      base::SupportsUserData* context_user_data,
      const url::Origin& origin);

  std::optional<std::string> GetBestFitLanguageCode(
      std::string requested_language);

  // Returns a delay upon initial translator creation to safeguard against
  // fingerprinting resulting from timing translator creation duration.
  //
  // The delay is triggered when the `availability()` of the translation
  // evaluates to "downloadable", even though all required resources for
  // translation have already been downloaded and available.
  //
  // Overridden for testing.
  virtual base::TimeDelta GetTranslatorDownloadDelay();
  virtual component_updater::ComponentUpdateService*
  GetComponentUpdateService();

  // Returns whether the "crash" language code is allowed. This is used for
  // testing.
  virtual bool CrashesAllowed();

  void CreateTranslatorImpl(
      mojo::PendingRemote<
          blink::mojom::TranslationManagerCreateTranslatorClient> client,
      const std::string& source_language,
      const std::string& target_language);

  // Dictionary keys for the `INITIALIZED_TRANSLATIONS` website setting.
  // Schema (per origin):
  // {
  //  ...
  //   "<source language code>" : { "<target language code>", ... }
  //   "en" : { "es", "fr", ... }
  //   "ja" : { "fr", "de", ... }
  //  ...
  // }
  base::Value GetInitializedTranslationsValue();

  // Determine if a translator has been initialized for the given languages.
  bool HasInitializedTranslator(const std::string& source_language,
                                const std::string& target_language);

  // Set the stored website setting value to the given dictionary of translation
  // language pairs.
  void SetTranslatorInitializedContentSetting(
      base::Value initialized_translations);

  // Updates the corresponding website setting to store information
  // for the given translation language pair, as needed.
  void SetInitializedTranslation(const std::string& source_language,
                                 const std::string& target_language);

  // `blink::mojom::TranslationManager` implementation.
  void CreateTranslator(
      mojo::PendingRemote<
          blink::mojom::TranslationManagerCreateTranslatorClient> client,
      blink::mojom::TranslatorCreateOptionsPtr options,
      bool add_fake_download_delay) override;

  void TranslationAvailable(blink::mojom::TranslatorLanguageCodePtr source_lang,
                            blink::mojom::TranslatorLanguageCodePtr target_lang,
                            TranslationAvailableCallback callback) override;

  OnDeviceTranslationServiceController& GetServiceController();

  // Instance of `TranslationManagerImpl` for testing.
  static TranslationManagerImpl* translation_manager_for_test_;

  const base::WeakPtr<content::BrowserContext> browser_context_;
  const url::Origin origin_;

  scoped_refptr<OnDeviceTranslationServiceController> service_controller_;
  mojo::UniqueReceiverSet<blink::mojom::Translator> translators_;
  mojo::ReceiverSet<blink::mojom::TranslationManager> receiver_set_;
  on_device_ai::AIModelDownloadProgressManager model_download_progress_manager_;

  base::WeakPtrFactory<TranslationManagerImpl> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

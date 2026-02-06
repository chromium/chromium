// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/mojom/download_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"
#include "url/origin.h"

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace on_device_translation {

class OnDeviceTranslationServiceController;

// The browser-side implementation of `blink::mojom::TranslationManager`, it
// is owned by a SupportsUserData (DocumentAssociatedData for frames,
// DedicatedWorkerHost, SharedWorkerHost and ServiceWorkerHost).
class TranslationManagerImpl : public base::SupportsUserData::Data,
                               public blink::mojom::TranslationManager {
 public:
  TranslationManagerImpl(
      base::PassKey<TranslationManagerImpl>,
      content::RenderProcessHost* process_host,
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      component_updater::ComponentUpdateService* component_update_service);
  TranslationManagerImpl(const TranslationManagerImpl&) = delete;
  TranslationManagerImpl& operator=(const TranslationManagerImpl&) = delete;

  ~TranslationManagerImpl() override;

  // Sets an instance of `TranslationManagerImpl` for testing.
  static base::AutoReset<TranslationManagerImpl*> SetForTesting(
      TranslationManagerImpl* manager);

  static void Bind(
      content::RenderProcessHost* process_host,
      content::BrowserContext* browser_context,
      base::SupportsUserData* context_user_data,
      const url::Origin& origin,
      component_updater::ComponentUpdateService* component_update_service,
      mojo::PendingReceiver<blink::mojom::TranslationManager> receiver);

 protected:
  TranslationManagerImpl(
      content::RenderProcessHost* process_host,
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      component_updater::ComponentUpdateService* component_update_service);

  content::BrowserContext* browser_context() { return browser_context_.get(); }
  content::RenderProcessHost* process_host() { return process_host_.get(); }

 private:
  static TranslationManagerImpl* GetOrCreate(
      content::RenderProcessHost* process_host,
      content::BrowserContext* browser_context,
      base::SupportsUserData* context_user_data,
      const url::Origin& origin,
      component_updater::ComponentUpdateService* component_update_service);

  std::optional<std::string> GetBestFitLanguageCode(
      std::string requested_language);

  // Returns whether the "crash" language code is allowed. This is used for
  // testing.
  virtual bool CrashesAllowed();

  // Determines if the Translator API has been accessed from a valid storage
  // partition.
  bool AccessedFromValidStoragePartition();

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

  // A set of language pairs that have been initialized for the current
  // document. This is used for origins that cannot persist content settings,
  // e.g. opaque origins or file schemes.
  std::set<std::pair<std::string, std::string>>
      transient_initialized_translations_;

  void CreateTranslatorImpl(
      mojo::PendingRemote<
          blink::mojom::TranslationManagerCreateTranslatorClient> client,
      const std::string& source_language,
      const std::string& target_language,
      std::unique_ptr<optimization_guide::OnDeviceModelDownloadProgressManager>
          model_download_progress_manager,
      base::expected<mojo::PendingRemote<mojom::Translator>,
                     blink::mojom::CreateTranslatorError> result);

  // `blink::mojom::TranslationManager` implementation.
  void CreateTranslator(
      mojo::PendingRemote<
          blink::mojom::TranslationManagerCreateTranslatorClient> client,
      blink::mojom::TranslatorCreateOptionsPtr options) override;

  void TranslationAvailable(blink::mojom::TranslatorLanguageCodePtr source_lang,
                            blink::mojom::TranslatorLanguageCodePtr target_lang,
                            TranslationAvailableCallback callback) override;

  OnDeviceTranslationServiceController& GetServiceController();

  // Instance of `TranslationManagerImpl` for testing.
  static TranslationManagerImpl* translation_manager_for_test_;

  raw_ptr<content::RenderProcessHost> process_host_;
  const base::WeakPtr<content::BrowserContext> browser_context_;
  const url::Origin origin_;

  scoped_refptr<OnDeviceTranslationServiceController> service_controller_;
  mojo::UniqueReceiverSet<blink::mojom::Translator> translators_;
  mojo::ReceiverSet<blink::mojom::TranslationManager> receiver_set_;
  raw_ptr<component_updater::ComponentUpdateService> component_update_service_;

  base::WeakPtrFactory<TranslationManagerImpl> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_IMPL_H_

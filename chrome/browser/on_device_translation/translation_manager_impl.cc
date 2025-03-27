// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/service_controller_manager.h"
#include "chrome/browser/on_device_translation/translation_manager_util.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/on_device_translation/translator.h"
#include "components/crx_file/id_util.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"

namespace on_device_translation {

namespace {

const void* kTranslationManagerUserDataKey = &kTranslationManagerUserDataKey;

using blink::mojom::CanCreateTranslatorResult;
using blink::mojom::CreateTranslatorError;
using blink::mojom::CreateTranslatorResult;
using blink::mojom::TranslationManagerCreateTranslatorClient;
using blink::mojom::TranslatorLanguageCode;
using blink::mojom::TranslatorLanguageCodePtr;
using content::BrowserContext;

}  // namespace

TranslationManagerImpl* TranslationManagerImpl::translation_manager_for_test_ =
    nullptr;

TranslationManagerImpl::TranslationManagerImpl(
    base::PassKey<TranslationManagerImpl>,
    BrowserContext* browser_context,
    const url::Origin& origin)
    : TranslationManagerImpl(browser_context, origin) {}

TranslationManagerImpl::TranslationManagerImpl(BrowserContext* browser_context,
                                               const url::Origin& origin)
    : browser_context_(browser_context->GetWeakPtr()), origin_(origin) {}

TranslationManagerImpl::~TranslationManagerImpl() = default;

// static
base::AutoReset<TranslationManagerImpl*> TranslationManagerImpl::SetForTesting(
    TranslationManagerImpl* manager) {
  return base::AutoReset<TranslationManagerImpl*>(
      &translation_manager_for_test_, manager);
}

// static
void TranslationManagerImpl::Bind(
    BrowserContext* browser_context,
    base::SupportsUserData* context_user_data,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
  auto* manager = GetOrCreate(browser_context, context_user_data, origin);
  CHECK(manager);
  CHECK_EQ(manager->origin_, origin);
  manager->receiver_set_.Add(manager, std::move(receiver));
}

// static
TranslationManagerImpl* TranslationManagerImpl::GetOrCreate(
    BrowserContext* browser_context,
    base::SupportsUserData* context_user_data,
    const url::Origin& origin) {
  // Use the testing instance of `TranslationManagerImpl*`, if it exists.
  if (translation_manager_for_test_) {
    return translation_manager_for_test_;
  }

  // TODO(crbug.com/322229993): Now that only one TranslationManager can be
  // bound, we can remove this.
  if (auto* manager = static_cast<TranslationManagerImpl*>(
          context_user_data->GetUserData(kTranslationManagerUserDataKey))) {
    return manager;
  }
  auto manager = std::make_unique<TranslationManagerImpl>(
      base::PassKey<TranslationManagerImpl>(), browser_context, origin);
  auto* manager_ptr = manager.get();
  context_user_data->SetUserData(kTranslationManagerUserDataKey,
                                 std::move(manager));
  return manager_ptr;
}

void TranslationManagerImpl::CanCreateTranslator(
    TranslatorLanguageCodePtr source_lang,
    TranslatorLanguageCodePtr target_lang,
    CanCreateTranslatorCallback callback) {
  const std::string source_language = source_lang->code;
  const std::string target_language = target_lang->code;

  RecordTranslationAPICallForLanguagePair("CanTranslate", source_language,
                                          target_language);

  if (!IsTranslatorAllowed(browser_context())) {
    std::move(callback).Run(CanCreateTranslatorResult::kNoDisallowedByPolicy);
    return;
  }

  if (!PassAcceptLanguagesCheck(GetAcceptLanguages(browser_context()),
                                source_language, target_language)) {
    std::move(callback).Run(
        CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed);
    return;
  }

  GetServiceController().CanTranslate(source_language, target_language,
                                      std::move(callback));
}

// Returns a delay upon initial translator creation to safeguard against
// fingerprinting resulting from timing translator creation duration.
//
// The delay is triggered when the `availability()` of the translation
// evaluates to "downloadable", even though all required resources for
// translation have already been downloaded and available.
base::TimeDelta TranslationManagerImpl::GetTranslatorDownloadDelay() {
  return base::RandTimeDelta(base::Seconds(2), base::Seconds(3));
}

component_updater::ComponentUpdateService*
TranslationManagerImpl::GetComponentUpdateService() {
  return g_browser_process->component_updater();
}

void TranslationManagerImpl::CreateTranslatorImpl(
    mojo::PendingRemote<TranslationManagerCreateTranslatorClient> client,
    const std::string& source_language,
    const std::string& target_language) {
  GetServiceController().CreateTranslator(
      source_language, target_language,
      base::BindOnce(
          [](base::WeakPtr<TranslationManagerImpl> self,
             mojo::PendingRemote<TranslationManagerCreateTranslatorClient>
                 client,
             const std::string& source_language,
             const std::string& target_language,
             base::expected<mojo::PendingRemote<mojom::Translator>,
                            CreateTranslatorError> result) {
            if (!client || !self) {
              // Request was aborted or the frame was destroyed. Note: Currently
              // aborting createTranslator() is not supported yet.
              // TODO(crbug.com/331735396): Support abort signal.
              return;
            }

            if (!result.has_value()) {
              mojo::Remote<TranslationManagerCreateTranslatorClient>(
                  std::move(client))
                  ->OnResult(CreateTranslatorResult::NewError(result.error()));
              return;
            }
            mojo::PendingRemote<::blink::mojom::Translator> blink_remote;
            self->translators_.Add(
                std::make_unique<Translator>(self->browser_context_,
                                             source_language, target_language,
                                             std::move(result.value())),
                blink_remote.InitWithNewPipeAndPassReceiver());
            mojo::Remote<TranslationManagerCreateTranslatorClient>(
                std::move(client))
                ->OnResult(CreateTranslatorResult::NewTranslator(
                    std::move(blink_remote)));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(client), source_language,
          target_language));
}

void TranslationManagerImpl::CreateTranslator(
    mojo::PendingRemote<TranslationManagerCreateTranslatorClient> client,
    blink::mojom::TranslatorCreateOptionsPtr options) {
  const std::string source_language = options->source_lang->code;
  const std::string target_language = options->target_lang->code;

  RecordTranslationAPICallForLanguagePair("Create", source_language,
                                          target_language);

  if (!IsTranslatorAllowed(browser_context())) {
    mojo::Remote(std::move(client))
        ->OnResult(CreateTranslatorResult::NewError(
            CreateTranslatorError::kDisallowedByPolicy));
    return;
  }

  if (!PassAcceptLanguagesCheck(GetAcceptLanguages(browser_context()),
                                source_language, target_language)) {
    mojo::Remote(std::move(client))
        ->OnResult(CreateTranslatorResult::NewError(
            CreateTranslatorError::kAcceptLanguagesCheckFailed));
    return;
  }

  if (options->observer_remote) {
    base::flat_set<std::string> component_ids = {
        component_updater::TranslateKitComponentInstallerPolicy::
            GetExtensionId()};
    std::set<LanguagePackKey> language_pack_keys =
        CalculateRequiredLanguagePacks(source_language, target_language);

    for (const LanguagePackKey& language_pack_key : language_pack_keys) {
      const LanguagePackComponentConfig& config =
          GetLanguagePackComponentConfig(language_pack_key);
      component_ids.insert(
          crx_file::id_util::GenerateIdFromHash(config.public_key_sha));
    }
    model_download_progress_manager_.AddObserver(
        GetComponentUpdateService(), std::move(options->observer_remote),
        std::move(component_ids));
  }

  base::OnceClosure create_translator =
      base::BindOnce(&TranslationManagerImpl::CreateTranslatorImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client),
                     source_language, target_language);

  TranslationAvailable(
      TranslatorLanguageCode::New(source_language),
      TranslatorLanguageCode::New(target_language),
      base::BindOnce(
          [](base::WeakPtr<TranslationManagerImpl> self,
             base::OnceClosure create_translator,
             CanCreateTranslatorResult result) {
            if (!self) {
              return;
            }

            if (base::FeatureList::IsEnabled(
                    blink::features::kTranslationAPIV1) &&
                result == CanCreateTranslatorResult::
                              kAfterDownloadTranslatorCreationRequired) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                  FROM_HERE, std::move(create_translator),
                  self->GetTranslatorDownloadDelay());
            } else {
              std::move(create_translator).Run();
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(create_translator)));
}

OnDeviceTranslationServiceController&
TranslationManagerImpl::GetServiceController() {
  if (!service_controller_) {
    ServiceControllerManager* manager =
        ServiceControllerManager::GetForBrowserContext(browser_context());
    CHECK(manager);
    service_controller_ = manager->GetServiceControllerForOrigin(origin_);
  }
  return *service_controller_;
}

void TranslationManagerImpl::TranslationAvailable(
    TranslatorLanguageCodePtr source_lang,
    TranslatorLanguageCodePtr target_lang,
    TranslationAvailableCallback callback) {
  const std::string source_language = std::move(source_lang->code);
  const std::string target_language = std::move(target_lang->code);

  RecordTranslationAPICallForLanguagePair("Availability", source_language,
                                          target_language);

  if (!IsTranslatorAllowed(browser_context())) {
    std::move(callback).Run(CanCreateTranslatorResult::kNoDisallowedByPolicy);
    return;
  }

  const std::vector<std::string_view> accept_languages =
      GetAcceptLanguages(browser_context());

  // TODO(crbug.com/385173766): Remove once V1 is launched.
  if (!PassAcceptLanguagesCheck(accept_languages, source_language,
                                target_language)) {
    std::move(callback).Run(
        CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed);
    return;
  }

  bool mask_readily_result =
      MaskReadilyResult(accept_languages, source_language, target_language);

  GetServiceController().CanTranslate(
      std::move(source_language), std::move(target_language),
      base::BindOnce(
          [](bool mask_readily_result, TranslationAvailableCallback callback,
             CanCreateTranslatorResult result) {
            if (result == CanCreateTranslatorResult::kReadily &&
                mask_readily_result) {
              // TODO(crbug.com/392073246): For translations containing a
              // language outside of English + the user's preferred (accept)
              // languages, check if a translator exists for the given origin
              // before returning the "readily" availability value for the
              // translation, instead of always returning an "after-download"
              // result.
              std::move(callback).Run(
                  CanCreateTranslatorResult::
                      kAfterDownloadTranslatorCreationRequired);
              return;
            }
            std::move(callback).Run(result);
          },
          mask_readily_result, std::move(callback)));
}
}  // namespace on_device_translation

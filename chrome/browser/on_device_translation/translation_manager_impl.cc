// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ai/ai_crx_component.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/service_controller_manager.h"
#include "chrome/browser/on_device_translation/translation_manager_util.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/on_device_translation/translator.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/crx_file/id_util.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

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
using content::RenderProcessHost;

// TODO(crbug.com/419848973): This is a workaround until the "he" language code
// is fully supported.
std::string SwitchLanguageCodeToIwIfHe(std::string language_code) {
  std::string language_subtag = language_code;
  int pos = language_code.find("-");
  if (pos != -1) {
    language_subtag.resize(pos);
  }
  if (language_subtag == "he") {
    language_code.replace(0, 2, "iw");
  }
  return language_code;
}

void RunTranslationAvailableCallbackWithMasking(
    bool mask_readily_result,
    std::string source_language,
    std::string target_language,
    TranslationManagerImpl::TranslationAvailableCallback callback,
    CanCreateTranslatorResult result) {
  if (result == CanCreateTranslatorResult::kReadily && mask_readily_result) {
    result =
        CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired;
  }
  std::move(callback).Run(result);
}

}  // namespace

TranslationManagerImpl* TranslationManagerImpl::translation_manager_for_test_ =
    nullptr;

TranslationManagerImpl::TranslationManagerImpl(
    base::PassKey<TranslationManagerImpl>,
    RenderProcessHost* process_host,
    BrowserContext* browser_context,
    const url::Origin& origin)
    : TranslationManagerImpl(process_host, browser_context, origin) {}

TranslationManagerImpl::TranslationManagerImpl(RenderProcessHost* process_host,
                                               BrowserContext* browser_context,
                                               const url::Origin& origin)
    : process_host_(process_host),
      browser_context_(browser_context->GetWeakPtr()),
      origin_(origin) {}

TranslationManagerImpl::~TranslationManagerImpl() = default;

// static
base::AutoReset<TranslationManagerImpl*> TranslationManagerImpl::SetForTesting(
    TranslationManagerImpl* manager) {
  return base::AutoReset<TranslationManagerImpl*>(
      &translation_manager_for_test_, manager);
}

// static
void TranslationManagerImpl::Bind(
    RenderProcessHost* process_host,
    BrowserContext* browser_context,
    base::SupportsUserData* context_user_data,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
  auto* manager =
      GetOrCreate(process_host, browser_context, context_user_data, origin);
  CHECK(manager);
  CHECK_EQ(manager->origin_, origin);
  manager->receiver_set_.Add(manager, std::move(receiver));
}

// static
TranslationManagerImpl* TranslationManagerImpl::GetOrCreate(
    RenderProcessHost* process_host,
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
      base::PassKey<TranslationManagerImpl>(), process_host, browser_context,
      origin);
  auto* manager_ptr = manager.get();
  context_user_data->SetUserData(kTranslationManagerUserDataKey,
                                 std::move(manager));
  return manager_ptr;
}

bool TranslationManagerImpl::AccessedFromValidStoragePartition() {
  if (process_host()->GetStoragePartition() !=
      browser_context()->GetDefaultStoragePartition()) {
    return !origin_.GetURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

base::Value TranslationManagerImpl::GetInitializedTranslationsValue() {
  return HostContentSettingsMapFactory::GetForProfile(browser_context())
      ->GetWebsiteSetting(origin_.GetURL(), origin_.GetURL(),
                          ContentSettingsType::INITIALIZED_TRANSLATIONS,
                          /*info=*/nullptr);
}

bool TranslationManagerImpl::HasInitializedTranslator(
    const std::string& source_language,
    const std::string& target_language) {
  const GURL url = origin_.GetURL();
  if (!url.is_valid() || url.SchemeIsFile()) {
    return transient_initialized_translations_.contains(
        {source_language, target_language});
  }

  base::Value initialized_translations_value =
      GetInitializedTranslationsValue();
  if (initialized_translations_value.is_dict()) {
    return initialized_translations_value.GetDict()
        .EnsureList(source_language)
        ->contains(target_language);
  }
  return false;
}

void TranslationManagerImpl::SetTranslatorInitializedContentSetting(
    base::Value initialized_translations) {
  HostContentSettingsMapFactory::GetForProfile(browser_context())
      ->SetWebsiteSettingDefaultScope(
          origin_.GetURL(), origin_.GetURL(),
          ContentSettingsType::INITIALIZED_TRANSLATIONS,
          std::move(initialized_translations));
}

void TranslationManagerImpl::SetInitializedTranslation(
    const std::string& source_language,
    const std::string& target_language) {
  const GURL url = origin_.GetURL();
  if (!url.is_valid() || url.SchemeIsFile()) {
    transient_initialized_translations_.insert(
        {source_language, target_language});
    return;
  }

  base::Value initialized_translations_value =
      GetInitializedTranslationsValue();

  // Initialize a dictionary to store data, if none exists.
  if (!initialized_translations_value.is_dict()) {
    initialized_translations_value = base::Value(base::Value::Dict());
  }

  // Update or initialize the list of targets for the source language.
  base::Value::List* target_languages_list =
      initialized_translations_value.GetDict().EnsureList(source_language);
  if (!target_languages_list->contains(target_language)) {
    target_languages_list->Append(target_language);
  }
  SetTranslatorInitializedContentSetting(
      std::move(initialized_translations_value));
}

std::optional<std::string> TranslationManagerImpl::GetBestFitLanguageCode(
    std::string requested_language) {
  // The "crash" code is only allowed in testing. This code triggers the mock
  // TranslateKit lib to crash, so that we can test graceful handling of
  // TranslateKit crashes.
  if (CrashesAllowed() && requested_language == "crash") {
    return requested_language;
  }
  std::string best_fit =
      SwitchLanguageCodeToIwIfHe(std::move(requested_language));
  return LookupMatchingLocaleByBestFit(kSupportedLanguageCodes,
                                       std::move(best_fit));
}

base::TimeDelta TranslationManagerImpl::GetTranslatorDownloadDelay() {
  return base::RandTimeDelta(base::Seconds(2), base::Seconds(3));
}

component_updater::ComponentUpdateService*
TranslationManagerImpl::GetComponentUpdateService() {
  return g_browser_process->component_updater();
}

bool TranslationManagerImpl::CrashesAllowed() {
  return false;
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
                  ->OnResult(CreateTranslatorResult::NewError(result.error()),
                             nullptr, nullptr);
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
                               std::move(blink_remote)),
                           TranslatorLanguageCode::New(source_language),
                           TranslatorLanguageCode::New(target_language));

            // TODO(crbug.com/414393698): Ensure stored WebsiteSetting is not
            // updated when create is aborted prior to download completion.
            //
            // Update the corresponding website setting if a translator has
            // been initialized as a result of translator creation.
            if (!self->HasInitializedTranslator(source_language,
                                                target_language)) {
              self->SetInitializedTranslation(source_language, target_language);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(client), source_language,
          target_language));
}

void TranslationManagerImpl::CreateTranslator(
    mojo::PendingRemote<TranslationManagerCreateTranslatorClient> client,
    blink::mojom::TranslatorCreateOptionsPtr options,
    bool add_fake_download_delay) {
  std::optional<std::string> maybe_source_language =
      GetBestFitLanguageCode(options->source_lang->code);
  std::optional<std::string> maybe_target_language =
      GetBestFitLanguageCode(options->target_lang->code);

  // TranslationAvailable should have been called on these language codes which
  // has already verified that a best fit language code exists, but if the
  // renderer is compromised, the CreateTranslator mojo function could be called
  // directly with invalid values.
  if (!maybe_source_language.has_value() ||
      !maybe_target_language.has_value()) {
    mojo::Remote(std::move(client))
        ->OnResult(CreateTranslatorResult::NewError(
                       CreateTranslatorError::kFailedToCreateTranslator),
                   nullptr, nullptr);
    return;
  }

  std::string source_language = *std::move(maybe_source_language);
  std::string target_language = *std::move(maybe_target_language);

  RecordTranslationAPICallForLanguagePair("Create", source_language,
                                          target_language);

  if (!IsTranslatorAllowed(browser_context())) {
    mojo::Remote(std::move(client))
        ->OnResult(CreateTranslatorResult::NewError(
                       CreateTranslatorError::kDisallowedByPolicy),
                   nullptr, nullptr);
    return;
  }

  if (!AccessedFromValidStoragePartition()) {
    mojo::Remote(std::move(client))
        ->OnResult(CreateTranslatorResult::NewError(
                       CreateTranslatorError::kInvalidStoragePartition),
                   nullptr, nullptr);
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
        std::move(options->observer_remote),
        on_device_ai::AICrxComponent::FromComponentIds(
            GetComponentUpdateService(), std::move(component_ids)));
  }

  base::OnceClosure create_translator =
      base::BindOnce(&TranslationManagerImpl::CreateTranslatorImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(client),
                     source_language, target_language);

  if (add_fake_download_delay) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(create_translator), GetTranslatorDownloadDelay());
  } else {
    std::move(create_translator).Run();
  }
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
  std::optional<std::string> maybe_source_language =
      GetBestFitLanguageCode(std::move(source_lang->code));
  std::optional<std::string> maybe_target_language =
      GetBestFitLanguageCode(std::move(target_lang->code));

  if (!maybe_source_language.has_value() ||
      !maybe_target_language.has_value()) {
    std::move(callback).Run(CanCreateTranslatorResult::kNoNotSupportedLanguage);
    return;
  }

  std::string source_language = *std::move(maybe_source_language);
  std::string target_language = *std::move(maybe_target_language);

  RecordTranslationAPICallForLanguagePair("Availability", source_language,
                                          target_language);

  if (!IsTranslatorAllowed(browser_context())) {
    std::move(callback).Run(CanCreateTranslatorResult::kNoDisallowedByPolicy);
    return;
  }

  if (!AccessedFromValidStoragePartition()) {
    std::move(callback).Run(
        CanCreateTranslatorResult::kNoInvalidStoragePartition);
    return;
  }

  const std::vector<std::string_view> accept_languages =
      GetAcceptLanguages(browser_context());

  bool are_source_and_target_accept_or_english =
      (IsInAcceptLanguage(accept_languages, source_language) ||
       l10n_util::GetLanguage(source_language) == "en") &&
      (IsInAcceptLanguage(accept_languages, target_language) ||
       l10n_util::GetLanguage(target_language) == "en");

  bool mask_readily_result =
      !HasInitializedTranslator(source_language, target_language) &&
      !are_source_and_target_accept_or_english;

  GetServiceController().CanTranslate(
      std::move(source_language), std::move(target_language),
      base::BindOnce(&RunTranslationAvailableCallbackWithMasking,
                     mask_readily_result, source_language, target_language,
                     std::move(callback)));
}
}  // namespace on_device_translation

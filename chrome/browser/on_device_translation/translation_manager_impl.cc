// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/browser/on_device_translation/component_manager.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/service_controller_manager.h"
#include "chrome/browser/on_device_translation/translation_metrics.h"
#include "chrome/browser/on_device_translation/translator.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace on_device_translation {

namespace {

const void* kTranslationManagerUserDataKey = &kTranslationManagerUserDataKey;

using blink::mojom::TranslationAvailability;
using blink::mojom::TranslatorLanguageCode;
using blink::mojom::TranslatorLanguageCodePtr;

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang) {
  const std::string normalized_lang = l10n_util::GetLanguage(lang);
  return std::find_if(accept_languages.begin(), accept_languages.end(),
                      [&](const std::string_view& lang) {
                        return l10n_util::GetLanguage(lang) == normalized_lang;
                      }) != accept_languages.end();
}

bool IsSupportedPopularLanguage(const std::string& lang) {
  const std::optional<SupportedLanguage> supported_lang =
      ToSupportedLanguage(lang);
  if (!supported_lang) {
    return false;
  }
  return IsPopularLanguage(*supported_lang);
}

// The number of language categories in the availability matrix.
constexpr size_t kLanguageCategoriesSize = 8u;

// LanguageCategory is used to represent the language category in the
// availability matrix.
struct LanguageCategory {
  bool installed;
  bool preferred;
  bool popular;
};

// Returns the index of the language category in the availability matrix.
size_t GetLanguageCategoryIndex(bool installed, bool preferred, bool popular) {
  return (installed ? 0 : 4) + (preferred ? 0 : 2) + (popular ? 0 : 1);
}

// Creates the language category list for the availability matrix.
std::vector<LanguageCategory> CreateLanguageCategoryList() {
  std::vector<LanguageCategory> list;
  list.reserve(kLanguageCategoriesSize);
  for (bool installed : {true, false}) {
    for (bool preferred : {true, false}) {
      for (bool popular : {true, false}) {
        CHECK_EQ(GetLanguageCategoryIndex(installed, preferred, popular),
                 list.size());
        list.emplace_back(LanguageCategory{
            .installed = installed,
            .preferred = preferred,
            .popular = popular,
        });
      }
    }
  }
  return list;
}

// Creates the language categories for the availability matrix.
// The language categories are stored in the following order:
//   0. Installed and preferred popular languages
//   1. Installed and preferred non-popular languages
//   2. Installed and non-preferred popular languages
//   3. Installed and non-preferred non-popular languages
//   4. Not installed and preferred popular languages
//   5. Not installed and preferred non-popular languages
//   6. Not installed and non-preferred popular languages
//   7. Not installed and non-preferred non-popular languages
// Note: `preferred` means that the language is in the user's accept language.
std::vector<std::vector<TranslatorLanguageCodePtr>> CreateLanguageCategories(
    const std::vector<std::string_view>& accept_languages,
    const std::set<LanguagePackKey>& installed_packs,
    bool is_en_preferred) {
  std::vector<std::vector<TranslatorLanguageCodePtr>> language_categories(
      kLanguageCategoriesSize);
  language_categories[GetLanguageCategoryIndex(/*installed=*/true,
                                               is_en_preferred,
                                               /*popular=*/true)]
      .emplace_back(TranslatorLanguageCode::New("en"));

  for (const auto& it : kLanguagePackComponentConfigMap) {
    const LanguagePackKey key = it.first;
    const SupportedLanguage supported_language =
        NonEnglishSupportedLanguageFromLanguagePackKey(key);
    const std::string_view language_code = ToLanguageCode(supported_language);
    const bool installed = installed_packs.contains(key);
    const bool preferred = IsInAcceptLanguage(accept_languages, language_code);
    const bool popular = IsPopularLanguage(supported_language);
    const size_t index =
        GetLanguageCategoryIndex(installed, preferred, popular);
    language_categories[index].push_back(
        TranslatorLanguageCode::New(std::string(language_code)));
  }
  return language_categories;
}

// Calculates the translation availability for the given source and target
// language categories.
TranslationAvailability CalculateTranslationAvailability(
    const LanguageCategory& source,
    const LanguageCategory& target,
    bool accept_languages_check_enabled,
    size_t installable_package_count) {
  if (accept_languages_check_enabled) {
    // If both the source and the destination language are not in the user's
    // accept language, the translation is not available.
    if (!(source.preferred || target.preferred)) {
      return TranslationAvailability::kNo;
    }
    // If the languages which is not in the user's accept language is not a
    // popular language, the translation is not available.
    if ((!source.preferred && !source.popular) ||
        (!target.preferred && !target.popular)) {
      return TranslationAvailability::kNo;
    }
  }

  // If both the source and the destination language are installed, the
  // translation is available.
  if (source.installed && target.installed) {
    return TranslationAvailability::kReadily;
  }
  // If both the source and the destination language are not installed, that
  // means the user has to download the two language packs.
  if (!source.installed && !target.installed) {
    // If the user can download two language packs, the translation is available
    // after download, otherwise it is not available.
    return installable_package_count >= 2
               ? TranslationAvailability::kAfterDownload
               : TranslationAvailability::kNo;
  }

  // If one of the source or the destination language is installed, that means
  // the user only needs to download one language pack.
  // So if the user can download one language pack, the translation is available
  // after download, otherwise it is not available.
  return installable_package_count >= 1
             ? TranslationAvailability::kAfterDownload
             : TranslationAvailability::kNo;
}

// Creates the availability matrix for each language category.
std::vector<std::vector<TranslationAvailability>> CreateAvailabilityMatrix(
    bool accept_languages_check_enabled,
    size_t installable_package_count) {
  const std::vector<LanguageCategory> categories = CreateLanguageCategoryList();
  std::vector<std::vector<TranslationAvailability>> matrix;
  matrix.reserve(kLanguageCategoriesSize);
  for (const auto& source : categories) {
    std::vector<TranslationAvailability> availability_row;
    availability_row.reserve(kLanguageCategoriesSize);
    for (auto target : categories) {
      availability_row.emplace_back(CalculateTranslationAvailability(
          source, target, accept_languages_check_enabled,
          installable_package_count));
    }
    matrix.emplace_back(std::move(availability_row));
  }
  return matrix;
}

}  // namespace

TranslationManagerImpl::TranslationManagerImpl(
    base::PassKey<TranslationManagerImpl>,
    content::BrowserContext* browser_context,
    const url::Origin& origin)
    : browser_context_(browser_context->GetWeakPtr()), origin_(origin) {}

TranslationManagerImpl::~TranslationManagerImpl() = default;

// static
void TranslationManagerImpl::Bind(
    content::BrowserContext* browser_context,
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
    content::BrowserContext* browser_context,
    base::SupportsUserData* context_user_data,
    const url::Origin& origin) {
  // Currently two TranslationManagers can be bound, for self.ai.translator and
  // for self.translator.
  // TODO(crbug.com/322229993): Remove this when we delete the legacy Translator
  // API.
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
    blink::mojom::TranslatorLanguageCodePtr source_lang,
    blink::mojom::TranslatorLanguageCodePtr target_lang,
    CanCreateTranslatorCallback callback) {
  CHECK(browser_context_);
  PrefService* profile_pref =
      Profile::FromBrowserContext(browser_context_.get())->GetPrefs();
  RecordTranslationAPICallForLanguagePair("CanTranslate", source_lang->code,
                                          target_lang->code);
  if (!profile_pref->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    std::move(callback).Run(
        blink::mojom::CanCreateTranslatorResult::kNoDisallowedByPolicy);
    return;
  }
  if (!PassAcceptLanguagesCheck(
          profile_pref->GetString(language::prefs::kAcceptLanguages),
          source_lang->code, target_lang->code)) {
    std::move(callback).Run(
        blink::mojom::CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed);
    return;
  }
  GetServiceController().CanTranslate(source_lang->code, target_lang->code,
                                      std::move(callback));
}

void TranslationManagerImpl::CreateTranslator(
    mojo::PendingRemote<blink::mojom::TranslationManagerCreateTranslatorClient>
        client,
    blink::mojom::TranslatorCreateOptionsPtr options) {
  RecordTranslationAPICallForLanguagePair("Create", options->source_lang->code,
                                          options->target_lang->code);
  CHECK(browser_context_);
  PrefService* profile_pref =
      Profile::FromBrowserContext(browser_context_.get())->GetPrefs();
  if (!profile_pref->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    mojo::Remote(std::move(client))
        ->OnResult(blink::mojom::CreateTranslatorResult::NewError(
            blink::mojom::CreateTranslatorError::kDisallowedByPolicy));
    return;
  }
  if (!PassAcceptLanguagesCheck(
          profile_pref->GetString(language::prefs::kAcceptLanguages),
          options->source_lang->code, options->target_lang->code)) {
    mojo::Remote(std::move(client))
        ->OnResult(blink::mojom::CreateTranslatorResult::NewError(
            blink::mojom::CreateTranslatorError::kAcceptLanguagesCheckFailed));
    return;
  }
  GetServiceController().CreateTranslator(
      options->source_lang->code, options->target_lang->code,
      base::BindOnce(
          [](base::WeakPtr<TranslationManagerImpl> self,
             mojo::PendingRemote<
                 blink::mojom::TranslationManagerCreateTranslatorClient> client,
             const std::string& source_lang, const std::string& target_lang,
             base::expected<mojo::PendingRemote<mojom::Translator>,
                            blink::mojom::CreateTranslatorError> result) {
            if (!client || !self) {
              // Request was aborted or the frame was destroyed. Note: Currently
              // aborting createTranslator() is not supported yet.
              // TODO(crbug.com/331735396): Support abort signal.
              return;
            }
            if (!result.has_value()) {
              mojo::Remote<
                  blink::mojom::TranslationManagerCreateTranslatorClient>(
                  std::move(client))
                  ->OnResult(blink::mojom::CreateTranslatorResult::NewError(
                      result.error()));
              return;
            }
            mojo::PendingRemote<::blink::mojom::Translator> blink_remote;
            self->translators_.Add(
                std::make_unique<Translator>(self->browser_context_,
                                             source_lang, target_lang,
                                             std::move(result.value())),
                blink_remote.InitWithNewPipeAndPassReceiver());
            mojo::Remote<
                blink::mojom::TranslationManagerCreateTranslatorClient>(
                std::move(client))
                ->OnResult(blink::mojom::CreateTranslatorResult::NewTranslator(
                    std::move(blink_remote)));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(client),
          options->source_lang->code, options->target_lang->code));
}

// static
bool TranslationManagerImpl::PassAcceptLanguagesCheck(
    const std::string& accept_languages_str,
    const std::string& source_lang,
    const std::string& target_lang) {
  if (!kTranslationAPIAcceptLanguagesCheck.Get()) {
    return true;
  }
  // When the TranslationAPIAcceptLanguagesCheck feature is enabled, the
  // Translation API will fail if neither the source nor destination language is
  // in the AcceptLanguages. This is intended to mitigate privacy concerns.
  const std::vector<std::string_view> accept_languages =
      base::SplitStringPiece(accept_languages_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  // TODO(crbug.com/371899260): Implement better language code handling.

  // One of the source or the destination language must be in the user's accept
  // language.
  const bool source_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, source_lang);
  const bool target_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, target_lang);
  if (!(source_lang_is_in_accept_langs || target_lang_is_in_accept_langs)) {
    return false;
  }

  // The other language must be a popular language.
  if (!source_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(source_lang)) {
    return false;
  }
  if (!target_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(target_lang)) {
    return false;
  }
  return true;
}

void TranslationManagerImpl::GetTranslatorAvailabilityInfo(
    GetTranslatorAvailabilityInfoCallback callback) {
  auto info = blink::mojom::TranslatorAvailabilityInfo::New();
  PrefService* profile_pref =
      Profile::FromBrowserContext(browser_context_.get())->GetPrefs();

  // Check if disabled by policy.
  if (!profile_pref->GetBoolean(prefs::kTranslatorAPIAllowed)) {
    info->availability = TranslationAvailability::kNo;
    std::move(callback).Run(std::move(info));
    return;
  }

  const std::string accept_languages_str =
      profile_pref->GetString(language::prefs::kAcceptLanguages);
  const std::vector<std::string_view> accept_languages =
      base::SplitStringPiece(accept_languages_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  const std::set<LanguagePackKey> installed_packs =
      ComponentManager::GetInstalledLanguagePacks();
  info->language_categories = CreateLanguageCategories(
      accept_languages, installed_packs,
      /*is_en_preferred*/ IsInAcceptLanguage(accept_languages, "en"));
  info->language_availability_matrix = CreateAvailabilityMatrix(
      /*accept_languages_check_enabled*/ kTranslationAPIAcceptLanguagesCheck
          .Get(),
      GetInstallablePackageCount(installed_packs.size()));
  info->availability = ComponentManager::GetTranslateKitLibraryPath().empty()
                           ? TranslationAvailability::kAfterDownload
                           : TranslationAvailability::kReadily;
  std::move(callback).Run(std::move(info));
}

OnDeviceTranslationServiceController&
TranslationManagerImpl::GetServiceController() {
  if (!service_controller_) {
    ServiceControllerManager* manager =
        ServiceControllerManager::GetForBrowserContext(browser_context_.get());
    CHECK(manager);
    service_controller_ = manager->GetServiceControllerForOrigin(origin_);
  }
  return *service_controller_;
}

}  // namespace on_device_translation

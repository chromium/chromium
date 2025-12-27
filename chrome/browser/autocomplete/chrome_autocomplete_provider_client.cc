// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/cstring_view.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/provider_state_service_factory.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/common/pref_names.h"
#include "components/history_clusters/core/features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"
#include "chrome/browser/autocomplete/unscoped_extension_provider_delegate_impl.h"
#include "extensions/common/extension_features.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/lens/lens_overlay_invocation_source.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/autocomplete/autocomplete_scoring_model_service_factory.h"
#include "chrome/browser/autocomplete/on_device_tail_model_service_factory.h"
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"
#include "components/omnibox/browser/on_device_tail_model_service.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace {

#if !BUILDFLAG(IS_ANDROID)
// This list should be kept in sync with chrome/common/webui_url_constants.h.
// Only include useful sub-pages, confirmation alerts are not useful.
constexpr auto kChromeSettingsSubPages = std::to_array<base::cstring_view>({
    chrome::kAddressesSubPage,
    chrome::kAutofillSubPage,
    chrome::kClearBrowserDataSubPage,
    chrome::kContentSettingsSubPage,
    chrome::kLanguageOptionsSubPage,
    chrome::kPasswordManagerSubPage,
    chrome::kPaymentsSubPage,
    chrome::kResetProfileSettingsSubPage,
    chrome::kSearchEnginesSubPage,
    chrome::kSyncSetupSubPage,
#if !BUILDFLAG(IS_CHROMEOS)
    chrome::kImportDataSubPage,
    chrome::kManageProfileSubPage,
    chrome::kPeopleSubPage,
#endif  // !BUILDFLAG(IS_CHROMEOS)
});

content::WebContents* GetWebContents(
    const ChromeAutocompleteProviderClient::WebContentsGetter&
        web_contents_getter) {
  return web_contents_getter ? web_contents_getter.Run() : nullptr;
}

LensSearchController* GetLensSearchController(
    content::WebContents* web_contents) {
  return web_contents ? LensSearchController::FromTabWebContents(web_contents)
                      : nullptr;
}

lens::LensSearchboxController* GetLensSearchboxController(
    content::WebContents* web_contents) {
  if (auto* lens_search_controller = GetLensSearchController(web_contents)) {
    return lens_search_controller->lens_searchbox_controller();
  }
  return nullptr;
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Whether the given contextual search `feature` is enabled for the specified
// `country` and `locale`.
bool IsContextualSearchFeatureEnabled(const base::Feature& feature,
                                      const std::string& country,
                                      const std::string& locale) {
  // If the feature is overridden (e.g. via server-side config or command-line),
  // use that state.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverridden(feature.name)) {
    return base::FeatureList::IsEnabled(feature);
  }

  if (!g_browser_process) {
    return false;
  }

  auto* variations_service = g_browser_process->variations_service();
  auto* features = g_browser_process->GetFeatures();

  if (!variations_service || !features) {
    return false;
  }

  return variations_service->GetStoredPermanentCountry() == country &&
         features->application_locale_storage() &&
         features->application_locale_storage()->Get() == locale;
}

}  // namespace

ChromeAutocompleteProviderClient::ChromeAutocompleteProviderClient(
    Profile* profile)
    : ChromeAutocompleteProviderClient(profile, base::NullCallback()) {}

ChromeAutocompleteProviderClient::ChromeAutocompleteProviderClient(
    Profile* profile,
    WebContentsGetter web_contents_getter)
    : profile_(profile),
      web_contents_getter_(std::move(web_contents_getter)),
      scheme_classifier_(profile),
      url_consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewAnonymizedDataCollectionConsentHelper(profile_->GetPrefs())),
      personalized_url_consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedDataCollectionConsentHelper(
                  SyncServiceFactory::GetForProfile(profile_))),
      tab_matcher_(GetTemplateURLService(), profile_),
      storage_partition_(nullptr),
      omnibox_triggered_feature_service_(
          std::make_unique<OmniboxTriggeredFeatureService>()) {
  pedal_provider_ = std::make_unique<OmniboxPedalProvider>(
      *this,
      GetPedalImplementations(profile_->IsIncognitoProfile(),
                              profile_->IsGuestSession(), /*testing=*/false));
}

ChromeAutocompleteProviderClient::~ChromeAutocompleteProviderClient() = default;

scoped_refptr<network::SharedURLLoaderFactory>
ChromeAutocompleteProviderClient::GetURLLoaderFactory() {
  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

PrefService* ChromeAutocompleteProviderClient::GetPrefs() const {
  return profile_->GetPrefs();
}

PrefService* ChromeAutocompleteProviderClient::GetLocalState() {
  return g_browser_process->local_state();
}

std::string ChromeAutocompleteProviderClient::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

const AutocompleteSchemeClassifier&
ChromeAutocompleteProviderClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
ChromeAutocompleteProviderClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
}

history::HistoryService* ChromeAutocompleteProviderClient::GetHistoryService() {
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

history_clusters::HistoryClustersService*
ChromeAutocompleteProviderClient::GetHistoryClustersService() {
  return HistoryClustersServiceFactory::GetForBrowserContext(profile_);
}

history_embeddings::HistoryEmbeddingsService*
ChromeAutocompleteProviderClient::GetHistoryEmbeddingsService() {
  return HistoryEmbeddingsServiceFactory::GetForProfile(profile_);
}

scoped_refptr<history::TopSites>
ChromeAutocompleteProviderClient::GetTopSites() {
  return TopSitesFactory::GetForProfile(profile_);
}

bookmarks::BookmarkModel* ChromeAutocompleteProviderClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

history::URLDatabase* ChromeAutocompleteProviderClient::GetInMemoryDatabase() {
  history::HistoryService* history_service = GetHistoryService();

  // This method is called in unit test contexts where the HistoryService isn't
  // loaded.
  return history_service ? history_service->InMemoryDatabase() : nullptr;
}

InMemoryURLIndex* ChromeAutocompleteProviderClient::GetInMemoryURLIndex() {
  return InMemoryURLIndexFactory::GetForProfile(profile_);
}

TemplateURLService* ChromeAutocompleteProviderClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

const TemplateURLService*
ChromeAutocompleteProviderClient::GetTemplateURLService() const {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

DocumentSuggestionsService*
ChromeAutocompleteProviderClient::GetDocumentSuggestionsService() const {
  return DocumentSuggestionsServiceFactory::GetForProfile(
      profile_, /*create_if_necessary=*/true);
}

RemoteSuggestionsService*
ChromeAutocompleteProviderClient::GetRemoteSuggestionsService(
    bool create_if_necessary) const {
  return RemoteSuggestionsServiceFactory::GetForProfile(profile_,
                                                        create_if_necessary);
}

ZeroSuggestCacheService*
ChromeAutocompleteProviderClient::GetZeroSuggestCacheService() {
  return ZeroSuggestCacheServiceFactory::GetForProfile(profile_);
}

const ZeroSuggestCacheService*
ChromeAutocompleteProviderClient::GetZeroSuggestCacheService() const {
  return ZeroSuggestCacheServiceFactory::GetForProfile(profile_);
}

OmniboxPedalProvider* ChromeAutocompleteProviderClient::GetPedalProvider()
    const {
  // This may be null for systems that don't have Pedals (Android, e.g.).
  return pedal_provider_.get();
}

scoped_refptr<ShortcutsBackend>
ChromeAutocompleteProviderClient::GetShortcutsBackend() {
  return ShortcutsBackendFactory::GetForProfile(profile_);
}

scoped_refptr<ShortcutsBackend>
ChromeAutocompleteProviderClient::GetShortcutsBackendIfExists() {
  return ShortcutsBackendFactory::GetForProfileIfExists(profile_);
}

std::unique_ptr<KeywordExtensionsDelegate>
ChromeAutocompleteProviderClient::GetKeywordExtensionsDelegate(
    KeywordProvider* keyword_provider) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return std::make_unique<KeywordExtensionsDelegateImpl>(profile_,
                                                         keyword_provider);
#else
  return nullptr;
#endif
}

std::unique_ptr<UnscopedExtensionProviderDelegate>
ChromeAutocompleteProviderClient::GetUnscopedExtensionProviderDelegate(
    UnscopedExtensionProvider* provider) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExperimentalOmniboxLabs));
  return std::make_unique<UnscopedExtensionProviderDelegateImpl>(profile_,
                                                                 provider);
#else
  return nullptr;
#endif
}

std::string ChromeAutocompleteProviderClient::GetAcceptLanguages() const {
  return profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
}

std::string
ChromeAutocompleteProviderClient::GetEmbedderRepresentationOfAboutScheme()
    const {
  return content::kChromeUIScheme;
}

std::vector<std::u16string> ChromeAutocompleteProviderClient::GetBuiltinURLs() {
  std::vector<std::u16string> builtins;
  const base::span<const base::cstring_view> url_hosts =
      chrome::ChromeURLHosts();
#if BUILDFLAG(IS_ANDROID)
  builtins.reserve(url_hosts.size());
#else
  builtins.reserve(url_hosts.size() + kChromeSettingsSubPages.size());
#endif

  for (base::cstring_view chrome_builtin_host : url_hosts) {
    builtins.push_back(base::ASCIIToUTF16(chrome_builtin_host));
  }
  std::ranges::sort(builtins);

#if !BUILDFLAG(IS_ANDROID)
  std::u16string settings(chrome::kChromeUISettingsHost16);
  settings += u"/";
  for (base::cstring_view chrome_settings_sub_page : kChromeSettingsSubPages) {
    builtins.push_back(settings + base::ASCIIToUTF16(chrome_settings_sub_page));
  }
#endif

  return builtins;
}

std::vector<std::u16string>
ChromeAutocompleteProviderClient::GetBuiltinsToProvideAsUserTypes() {
  std::vector<std::u16string> builtins_to_provide;
  builtins_to_provide.push_back(chrome::kChromeUIChromeURLsURL16);
  builtins_to_provide.push_back(chrome::kChromeUIFlagsURL16);
#if !BUILDFLAG(IS_ANDROID)
  builtins_to_provide.push_back(chrome::kChromeUISettingsURL16);
#endif
  builtins_to_provide.push_back(chrome::kChromeUIVersionURL16);
  return builtins_to_provide;
}

component_updater::ComponentUpdateService*
ChromeAutocompleteProviderClient::GetComponentUpdateService() {
  return g_browser_process->component_updater();
}

OmniboxTriggeredFeatureService*
ChromeAutocompleteProviderClient::GetOmniboxTriggeredFeatureService() const {
  return omnibox_triggered_feature_service_.get();
}

signin::IdentityManager* ChromeAutocompleteProviderClient::GetIdentityManager()
    const {
  return IdentityManagerFactory::GetForProfile(profile_);
}

AutocompleteScoringModelService*
ChromeAutocompleteProviderClient::GetAutocompleteScoringModelService() const {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return AutocompleteScoringModelServiceFactory::GetForProfile(profile_);
#else
  return nullptr;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

OnDeviceTailModelService*
ChromeAutocompleteProviderClient::GetOnDeviceTailModelService() const {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return OnDeviceTailModelServiceFactory::GetForProfile(profile_);
#else
  return nullptr;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

ProviderStateService*
ChromeAutocompleteProviderClient::GetProviderStateService() const {
  return ProviderStateServiceFactory::GetForProfile(profile_);
}

tab_groups::TabGroupSyncService*
ChromeAutocompleteProviderClient::GetTabGroupSyncService() const {
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_);
}

AimEligibilityService*
ChromeAutocompleteProviderClient::GetAimEligibilityService() const {
  return AimEligibilityServiceFactory::GetForProfile(profile_);
}

bool ChromeAutocompleteProviderClient::IsOffTheRecord() const {
  return profile_->IsOffTheRecord();
}

bool ChromeAutocompleteProviderClient::IsIncognitoProfile() const {
  return profile_->IsIncognitoProfile();
}

bool ChromeAutocompleteProviderClient::IsGuestSession() const {
  return profile_->IsGuestSession();
}

bool ChromeAutocompleteProviderClient::SearchSuggestEnabled() const {
#if BUILDFLAG(IS_CHROMEOS)
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled) &&
         (!g_browser_process->platform_part() ||
          !g_browser_process->platform_part()->essential_search_manager() ||
          !g_browser_process->platform_part()
               ->essential_search_manager()
               ->ShouldDisableSearchSuggest());
#else
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
#endif
}

bool ChromeAutocompleteProviderClient::AllowDeletingBrowserHistory() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

bool ChromeAutocompleteProviderClient::IsUrlDataCollectionActive() const {
  return url_consent_helper_->IsEnabled();
}

bool ChromeAutocompleteProviderClient::IsPersonalizedUrlDataCollectionActive()
    const {
  return personalized_url_consent_helper_->IsEnabled();
}

bool ChromeAutocompleteProviderClient::IsAuthenticated() const {
  const auto* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  return identity_manager && !identity_manager->GetAccountsInCookieJar()
                                  .GetPotentiallyInvalidSignedInAccounts()
                                  .empty();
}

bool ChromeAutocompleteProviderClient::IsSyncActive() const {
  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile_);
  return sync && sync->IsSyncFeatureActive();
}

std::string ChromeAutocompleteProviderClient::ProfileUserName() const {
  return profile_->GetProfileUserName();
}

void ChromeAutocompleteProviderClient::Classify(
    const std::u16string& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  AutocompleteClassifier* classifier = GetAutocompleteClassifier();
  DCHECK(classifier);
  classifier->Classify(text, prefer_keyword, allow_exact_keyword_match,
                       page_classification, match, alternate_nav_url);
}

void ChromeAutocompleteProviderClient::DeleteMatchingURLsForKeywordFromHistory(
    history::KeywordID keyword_id,
    const std::u16string& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void ChromeAutocompleteProviderClient::PrefetchImage(const GURL& url) {
  // Note: Android uses different image fetching mechanism to avoid
  // penalty of copying byte buffers from C++ to Java.
#if !BUILDFLAG(IS_ANDROID)
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  bitmap_fetcher_service->Prefetch(url);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutocompleteProviderClient::StartServiceWorker(
    const GURL& destination_url) {
  if (!SearchSuggestEnabled()) {
    return;
  }

  if (profile_->IsOffTheRecord()) {
    return;
  }

  content::StoragePartition* partition = storage_partition_;
  if (!partition) {
    partition = profile_->GetDefaultStoragePartition();
  }
  if (!partition) {
    return;
  }

  content::ServiceWorkerContext* context = partition->GetServiceWorkerContext();
  if (!context) {
    return;
  }

  context->StartServiceWorkerForNavigationHint(
      destination_url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(destination_url)),
      base::DoNothing());
}

const TabMatcher& ChromeAutocompleteProviderClient::GetTabMatcher() const {
  return tab_matcher_;
}

bool ChromeAutocompleteProviderClient::IsIncognitoModeAvailable() const {
  return IncognitoModePrefs::IsIncognitoAllowed(profile_);
}

bool ChromeAutocompleteProviderClient::IsSharingHubAvailable() const {
#if !BUILDFLAG(IS_ANDROID)
  return sharing_hub::SharingHubOmniboxEnabled(profile_);
#else
  return false;
#endif
}

bool ChromeAutocompleteProviderClient::IsHistoryEmbeddingsEnabled() const {
  return history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile_);
}

bool ChromeAutocompleteProviderClient::IsHistoryEmbeddingsSettingVisible()
    const {
  return history_embeddings::IsHistoryEmbeddingsSettingVisible(profile_);
}

bool ChromeAutocompleteProviderClient::IsLensEnabled() const {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* lens_search_controller =
          GetLensSearchController(GetWebContents(web_contents_getter_))) {
    // Guaranteed to exist if lens_search_controller is  not null.
    return lens_search_controller->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->GetFeatures()
        .lens_overlay_entry_point_controller()
        ->IsEnabled();
  }
#endif
  return false;
}

bool ChromeAutocompleteProviderClient::AreLensEntrypointsVisible() const {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* lens_search_controller =
          GetLensSearchController(GetWebContents(web_contents_getter_))) {
    // Guaranteed to exist if lens_search_controller is  not null.
    return lens_search_controller->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->GetFeatures()
        .lens_overlay_entry_point_controller()
        ->AreVisible();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return false;
}

std::optional<bool> ChromeAutocompleteProviderClient::IsPagePaywalled() const {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* web_contents = GetWebContents(web_contents_getter_)) {
    if (auto* tab_helper = OmniboxTabHelper::FromWebContents(web_contents)) {
      return tab_helper->IsPagePaywalled();
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return false;
}

bool ChromeAutocompleteProviderClient::ShouldSendContextualUrlSuggestParam()
    const {
  return IsContextualSearchFeatureEnabled(
      omnibox_feature_configs::ContextualSearch::kSendContextualUrlSuggestParam,
      /*country=*/"us", /*locale=*/"en-US");
}

bool ChromeAutocompleteProviderClient::ShouldSendPageTitleSuggestParam() const {
  return IsContextualSearchFeatureEnabled(
      omnibox_feature_configs::ContextualSearch::kSendPageTitleSuggestParam,
      /*country=*/"us", /*locale=*/"en-US");
}

bool ChromeAutocompleteProviderClient::IsOmniboxNextLensSearchChipEnabled()
    const {
#if !BUILDFLAG(IS_ANDROID)
  return omnibox::IsAimPopupEnabled(profile_) &&
         omnibox::kShowLensSearchChip.Get();
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

base::CallbackListSubscription
ChromeAutocompleteProviderClient::GetLensSuggestInputsWhenReady(
    LensOverlaySuggestInputsCallback callback) const {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* lens_searchbox_controller =
          GetLensSearchboxController(GetWebContents(web_contents_getter_))) {
    return lens_searchbox_controller->GetLensSuggestInputsWhenReady(
        std::move(callback));
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::nullopt);
  return {};
}

base::WeakPtr<AutocompleteProviderClient>
ChromeAutocompleteProviderClient::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ChromeAutocompleteProviderClient::OpenSharingHub() {
#if !BUILDFLAG(IS_ANDROID)
  if (BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
    chrome::ExecuteCommand(bwi, IDC_SHARING_HUB);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutocompleteProviderClient::NewIncognitoWindow() {
#if !BUILDFLAG(IS_ANDROID)
  chrome::NewIncognitoWindow(profile_);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutocompleteProviderClient::OpenIncognitoClearBrowsingDataDialog() {
#if !BUILDFLAG(IS_ANDROID)
  if (BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
    chrome::ShowIncognitoClearBrowsingDataDialog(bwi);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutocompleteProviderClient::CloseIncognitoWindows() {
#if !BUILDFLAG(IS_ANDROID)
  if (profile_->IsIncognitoProfile()) {
    BrowserList::CloseAllBrowsersWithIncognitoProfile(
        profile_, base::DoNothing(), base::DoNothing(), true);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool ChromeAutocompleteProviderClient::OpenJourneys(const std::string& query) {
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!bwi) {
    return false;
  }

  auto* const history_clusters_side_panel_coordinator =
      bwi->GetFeatures().history_clusters_side_panel_coordinator();
  if (history_clusters_side_panel_coordinator &&
      history_clusters_side_panel_coordinator->Show(query)) {
    return true;
  }

#endif  // !BUILDFLAG(IS_ANDROID)

  return false;
}

void ChromeAutocompleteProviderClient::OpenLensOverlay(bool show) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* lens_search_controller =
          GetLensSearchController(GetWebContents(web_contents_getter_))) {
    if (show) {
      lens_search_controller->OpenLensOverlay(
          lens::LensOverlayInvocationSource::kOmniboxPageAction);
    } else {
      // TODO(crbug.com/402497756): For prototyping, reusing the existing
      // omnibox entry point. However, for production, create a new invocation
      // source for this new entry point.
      lens_search_controller->StartContextualization(
          lens::LensOverlayInvocationSource::kOmnibox);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutocompleteProviderClient::IssueContextualSearchRequest(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* lens_search_controller =
          GetLensSearchController(GetWebContents(web_contents_getter_))) {
    lens_search_controller->IssueContextualSearchRequest(
        lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion,
        destination_url, match_type, is_zero_prefix_suggestion);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutocompleteProviderClient::PromptPageTranslation() {
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  content::WebContents* contents = nullptr;
  if (bwi) {
    contents = bwi->GetTabStripModel()->GetActiveWebContents();
  }
  if (contents) {
    ChromeTranslateClient* translate_client =
        ChromeTranslateClient::FromWebContents(contents);
    if (translate_client) {
      DCHECK_NE(nullptr, translate_client->GetTranslateManager());
      translate_client->GetTranslateManager()->ShowTranslateUI(
          /*auto_translate=*/true, /*triggered_from_menu=*/true);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

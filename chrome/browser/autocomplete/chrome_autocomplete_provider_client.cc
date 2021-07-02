// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <stddef.h>

#include <algorithm>

#include "base/callback_helpers.h"
#include "base/cxx17_backports.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_android_user_data.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/omnibox/jni_headers/ChromeAutocompleteProviderClient_jni.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#endif

#if defined(OS_ANDROID)
using chrome::android::ActivityType;
#endif

namespace {

#if defined(OS_ANDROID)
class AutocompleteClientTabAndroidUserData
    : public TabAndroidUserData<AutocompleteClientTabAndroidUserData>,
      public TabAndroid::Observer {
 public:
  ~AutocompleteClientTabAndroidUserData() override {
    tab_->RemoveObserver(this);
  }

  const GURL& GetStrippedURL() { return stripped_url_; }

  bool IsInitialized() { return initialized_; }

  void UpdateStrippedURL(const GURL& url,
                         TemplateURLService* template_url_service) {
    initialized_ = true;
    if (url.is_valid()) {
      stripped_url_ = AutocompleteMatch::GURLToStrippedGURL(
          url, AutocompleteInput(), template_url_service, std::u16string());
    }
  }

  // TabAndroid::Observer implementation
  void OnInitWebContents(TabAndroid* tab) override {
    tab->RemoveUserData(UserDataKey());
  }

 private:
  explicit AutocompleteClientTabAndroidUserData(TabAndroid* tab) : tab_(tab) {
    DCHECK(tab);
    tab->AddObserver(this);
  }
  friend class TabAndroidUserData<AutocompleteClientTabAndroidUserData>;

  TabAndroid* tab_;
  bool initialized_ = false;
  GURL stripped_url_;

  TAB_ANDROID_USER_DATA_KEY_DECL();
};
TAB_ANDROID_USER_DATA_KEY_IMPL(AutocompleteClientTabAndroidUserData)

#else  // defined(OS_ANDROID)
// This list should be kept in sync with chrome/common/webui_url_constants.h.
// Only include useful sub-pages, confirmation alerts are not useful.
const char* const kChromeSettingsSubPages[] = {
    chrome::kAddressesSubPage,        chrome::kAutofillSubPage,
    chrome::kClearBrowserDataSubPage, chrome::kContentSettingsSubPage,
    chrome::kLanguageOptionsSubPage,  chrome::kPasswordManagerSubPage,
    chrome::kPaymentsSubPage,         chrome::kResetProfileSettingsSubPage,
    chrome::kSearchEnginesSubPage,    chrome::kSyncSetupSubPage,
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    chrome::kCreateProfileSubPage,    chrome::kImportDataSubPage,
    chrome::kManageProfileSubPage,    chrome::kPeopleSubPage,
#endif
};
#endif  // defined(OS_ANDROID)

class AutocompleteClientWebContentsUserData
    : public content::WebContentsUserData<
          AutocompleteClientWebContentsUserData> {
 public:
  ~AutocompleteClientWebContentsUserData() override = default;

  int GetLastCommittedEntryIndex() { return last_committed_entry_index_; }
  const GURL& GetLastCommittedStrippedURL() {
    return last_committed_stripped_url_;
  }
  void UpdateLastCommittedStrippedURL(
      int last_committed_index,
      const GURL& last_committed_url,
      TemplateURLService* template_url_service) {
    if (last_committed_url.is_valid()) {
      last_committed_entry_index_ = last_committed_index;
      // Use blank input since we will re-use this stripped URL with other
      // inputs.
      last_committed_stripped_url_ = AutocompleteMatch::GURLToStrippedGURL(
          last_committed_url, AutocompleteInput(), template_url_service,
          std::u16string());
    }
  }

 private:
  explicit AutocompleteClientWebContentsUserData(
      content::WebContents* contents);
  friend class content::WebContentsUserData<
      AutocompleteClientWebContentsUserData>;

  int last_committed_entry_index_ = -1;
  GURL last_committed_stripped_url_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

AutocompleteClientWebContentsUserData::AutocompleteClientWebContentsUserData(
    content::WebContents*) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutocompleteClientWebContentsUserData)

}  // namespace

ChromeAutocompleteProviderClient::ChromeAutocompleteProviderClient(
    Profile* profile)
    : profile_(profile),
      scheme_classifier_(profile),
      url_consent_helper_(unified_consent::UrlKeyedDataCollectionConsentHelper::
                              NewPersonalizedDataCollectionConsentHelper(
                                  SyncServiceFactory::GetForProfile(profile_))),
      storage_partition_(nullptr),
      omnibox_triggered_feature_service_(
          std::make_unique<OmniboxTriggeredFeatureService>()) {
#if !defined(OS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  pedal_provider_ = std::make_unique<OmniboxPedalProvider>(*this, true);
#else
  pedal_provider_ = std::make_unique<OmniboxPedalProvider>(*this, false);
#endif
#endif
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

scoped_refptr<history::TopSites>
ChromeAutocompleteProviderClient::GetTopSites() {
  return TopSitesFactory::GetForProfile(profile_);
}

ntp_tiles::MostVisitedSites*
ChromeAutocompleteProviderClient::GetNtpMostVisitedSites() {
  if (!most_visited_sites_) {
    most_visited_sites_ =
        ChromeMostVisitedSitesFactory::NewForProfile(profile_);
  }
  return most_visited_sites_.get();
}

bookmarks::BookmarkModel* ChromeAutocompleteProviderClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

history::URLDatabase* ChromeAutocompleteProviderClient::GetInMemoryDatabase() {
  history::HistoryService* history_service = GetHistoryService();

  // This method is called in unit test contexts where the HistoryService isn't
  // loaded.
  return history_service ? history_service->InMemoryDatabase() : NULL;
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

RemoteSuggestionsService*
ChromeAutocompleteProviderClient::GetRemoteSuggestionsService(
    bool create_if_necessary) const {
  return RemoteSuggestionsServiceFactory::GetForProfile(profile_,
                                                        create_if_necessary);
}

DocumentSuggestionsService*
ChromeAutocompleteProviderClient::GetDocumentSuggestionsService(
    bool create_if_necessary) const {
  return DocumentSuggestionsServiceFactory::GetForProfile(profile_,
                                                          create_if_necessary);
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return std::make_unique<KeywordExtensionsDelegateImpl>(profile_,
                                                         keyword_provider);
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
  std::vector<std::string> chrome_builtins(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(chrome_builtins.begin(), chrome_builtins.end());

  std::vector<std::u16string> builtins;

  for (auto i(chrome_builtins.begin()); i != chrome_builtins.end(); ++i)
    builtins.push_back(base::ASCIIToUTF16(*i));

#if !defined(OS_ANDROID)
  std::u16string settings(base::ASCIIToUTF16(chrome::kChromeUISettingsHost) +
                          u"/");
  for (size_t i = 0; i < base::size(kChromeSettingsSubPages); i++) {
    builtins.push_back(settings +
                       base::ASCIIToUTF16(kChromeSettingsSubPages[i]));
  }
#endif

  return builtins;
}

std::vector<std::u16string>
ChromeAutocompleteProviderClient::GetBuiltinsToProvideAsUserTypes() {
  std::vector<std::u16string> builtins_to_provide;
  builtins_to_provide.push_back(
      base::ASCIIToUTF16(chrome::kChromeUIChromeURLsURL));
#if !defined(OS_ANDROID)
  builtins_to_provide.push_back(
      base::ASCIIToUTF16(chrome::kChromeUISettingsURL));
#endif
  builtins_to_provide.push_back(
      base::ASCIIToUTF16(chrome::kChromeUIVersionURL));
  return builtins_to_provide;
}

component_updater::ComponentUpdateService*
ChromeAutocompleteProviderClient::GetComponentUpdateService() {
  return g_browser_process->component_updater();
}

query_tiles::TileService*
ChromeAutocompleteProviderClient::GetQueryTileService() const {
  ProfileKey* profile_key = profile_->GetProfileKey();
  return query_tiles::TileServiceFactory::GetForKey(profile_key);
}

OmniboxTriggeredFeatureService*
ChromeAutocompleteProviderClient::GetOmniboxTriggeredFeatureService() const {
  return omnibox_triggered_feature_service_.get();
}

signin::IdentityManager* ChromeAutocompleteProviderClient::GetIdentityManager()
    const {
  return IdentityManagerFactory::GetForProfile(profile_);
}

bool ChromeAutocompleteProviderClient::IsOffTheRecord() const {
  return profile_->IsOffTheRecord();
}

bool ChromeAutocompleteProviderClient::SearchSuggestEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool ChromeAutocompleteProviderClient::AllowDeletingBrowserHistory() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

bool ChromeAutocompleteProviderClient::IsPersonalizedUrlDataCollectionActive()
    const {
  return url_consent_helper_->IsEnabled();
}

bool ChromeAutocompleteProviderClient::IsAuthenticated() const {
  const auto* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  return identity_manager &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
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
#if !defined(OS_ANDROID)
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  bitmap_fetcher_service->Prefetch(url);
#endif  // !defined(OS_ANDROID)
}

void ChromeAutocompleteProviderClient::StartServiceWorker(
    const GURL& destination_url) {
  if (!SearchSuggestEnabled())
    return;

  if (profile_->IsOffTheRecord())
    return;

  content::StoragePartition* partition = storage_partition_;
  if (!partition)
    partition = profile_->GetDefaultStoragePartition();
  if (!partition)
    return;

  content::ServiceWorkerContext* context = partition->GetServiceWorkerContext();
  if (!context)
    return;

  context->StartServiceWorkerForNavigationHint(destination_url,
                                               base::DoNothing());
}

bool ChromeAutocompleteProviderClient::IsTabOpenWithURL(
    const GURL& url,
    const AutocompleteInput* input) {
#if defined(OS_ANDROID)
  return GetTabOpenWithURL(url, input) != nullptr;
#else
  const AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;
  const GURL stripped_url = AutocompleteMatch::GURLToStrippedGURL(
      url, *input, GetTemplateURLService(), std::u16string());
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  content::WebContents* active_tab = nullptr;
  if (active_browser)
    active_tab = active_browser->tab_strip_model()->GetActiveWebContents();
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only look at same profile (and anonymity level).
    if (profile_ == browser->profile()) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        if (web_contents != active_tab &&
            IsStrippedURLEqualToWebContentsURL(stripped_url, web_contents))
          return true;
      }
    }
  }
  return false;
#endif  // defined(OS_ANDROID)
}

bool ChromeAutocompleteProviderClient::IsIncognitoModeAvailable() const {
  if (profile_->IsGuestSession()) {
    return false;
  }
  return IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
         IncognitoModePrefs::DISABLED;
}

void ChromeAutocompleteProviderClient::OnAutocompleteControllerResultReady(
    AutocompleteController* controller) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(profile_);

  // Prefetches result pages that the search provider marked as prefetchable.
  if (search_prefetch_service)
    search_prefetch_service->OnResultChanged(controller);
}

bool ChromeAutocompleteProviderClient::StrippedURLsAreEqual(
    const GURL& url1,
    const GURL& url2,
    const AutocompleteInput* input) const {
  AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;
  const TemplateURLService* template_url_service = GetTemplateURLService();
  return AutocompleteMatch::GURLToStrippedGURL(
             url1, *input, template_url_service, std::u16string()) ==
         AutocompleteMatch::GURLToStrippedGURL(
             url2, *input, template_url_service, std::u16string());
}

bool ChromeAutocompleteProviderClient::IsStrippedURLEqualToWebContentsURL(
    const GURL& stripped_url,
    content::WebContents* web_contents) {
  AutocompleteClientWebContentsUserData::CreateForWebContents(web_contents);
  AutocompleteClientWebContentsUserData* user_data =
      AutocompleteClientWebContentsUserData::FromWebContents(web_contents);
  DCHECK(user_data);
  if (user_data->GetLastCommittedEntryIndex() !=
      web_contents->GetController().GetLastCommittedEntryIndex()) {
    user_data->UpdateLastCommittedStrippedURL(
        web_contents->GetController().GetLastCommittedEntryIndex(),
        web_contents->GetLastCommittedURL(), GetTemplateURLService());
  }
  return stripped_url == user_data->GetLastCommittedStrippedURL();
}

#if defined(OS_ANDROID)
TabAndroid* ChromeAutocompleteProviderClient::GetTabOpenWithURL(
    const GURL& url,
    const AutocompleteInput* input) {
  const AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;
  const GURL stripped_url = AutocompleteMatch::GURLToStrippedGURL(
      url, *input, GetTemplateURLService(), std::u16string());

  std::vector<TabModel*> tab_models;
  for (TabModel* model : TabModelList::models()) {
    if (profile_ != model->GetProfile())
      continue;

    tab_models.push_back(model);
  }

  std::vector<TabAndroid*> all_tabs = GetAllHiddenAndNonCCTTabs(tab_models);

  for (TabAndroid* tab : all_tabs) {
    content::WebContents* web_contents = tab->web_contents();
    if (web_contents != nullptr) {
      if (IsStrippedURLEqualToWebContentsURL(stripped_url, web_contents))
        return tab;
    } else {
      // Browser did not load the tab yet after Chrome started. To avoid
      // reloading WebContents, we just compare URLs.
      AutocompleteClientTabAndroidUserData::CreateForTabAndroid(tab);
      AutocompleteClientTabAndroidUserData* user_data =
          AutocompleteClientTabAndroidUserData::FromTabAndroid(tab);
      DCHECK(user_data);
      if (!user_data->IsInitialized())
        user_data->UpdateStrippedURL(tab->GetURL(), GetTemplateURLService());

      const GURL tab_stripped_url = user_data->GetStrippedURL();
      if (tab_stripped_url == stripped_url)
        return tab;
    }
  }

  return nullptr;
}

std::vector<TabAndroid*>
ChromeAutocompleteProviderClient::GetAllHiddenAndNonCCTTabs(
    const std::vector<TabModel*>& tab_models) {
  if (tab_models.size() == 0)
    return std::vector<TabAndroid*>();

  JNIEnv* env = base::android::AttachCurrentThread();
  jclass tab_model_clazz = TabModelJniBridge::GetClazz(env);
  base::android::ScopedJavaLocalRef<jobjectArray> j_tab_model_array(
      env, env->NewObjectArray(tab_models.size(), tab_model_clazz, nullptr));
  // Get all the hidden and non CCT tabs. Filter the tabs in CCT tabmodel first.
  for (size_t i = 0; i < tab_models.size(); ++i) {
    ActivityType type = tab_models[i]->activity_type();
    if (type == ActivityType::kCustomTab ||
        type == ActivityType::kTrustedWebActivity) {
      continue;
    }
    env->SetObjectArrayElement(j_tab_model_array.obj(), i,
                               tab_models[i]->GetJavaObject().obj());
  }

  base::android::ScopedJavaLocalRef<jobjectArray> j_tabs =
      Java_ChromeAutocompleteProviderClient_getAllHiddenTabs(env,
                                                             j_tab_model_array);
  if (j_tabs.is_null())
    return std::vector<TabAndroid*>();

  return TabAndroid::GetAllNativeTabs(env, j_tabs);
}
#endif  // defined(OS_ANDROID)

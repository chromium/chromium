// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
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

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace {

#if !defined(OS_ANDROID)
// This list should be kept in sync with chrome/common/webui_url_constants.h.
// Only include useful sub-pages, confirmation alerts are not useful.
const char* const kChromeSettingsSubPages[] = {
    chrome::kAddressesSubPage,        chrome::kAutofillSubPage,
    chrome::kClearBrowserDataSubPage, chrome::kContentSettingsSubPage,
    chrome::kLanguageOptionsSubPage,  chrome::kPasswordManagerSubPage,
    chrome::kPaymentsSubPage,         chrome::kResetProfileSettingsSubPage,
    chrome::kSearchEnginesSubPage,    chrome::kSyncSetupSubPage,
#if !defined(OS_CHROMEOS)
    chrome::kCreateProfileSubPage,    chrome::kImportDataSubPage,
    chrome::kManageProfileSubPage,    chrome::kPeopleSubPage,
#endif
};
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
const char* const kChromeOSSettingsSubPages[] = {
    chrome::kAccessibilitySubPage, chrome::kBluetoothSubPage,
    chrome::kDateTimeSubPage,      chrome::kDisplaySubPage,
    chrome::kInternetSubPage,      chrome::kNativePrintingSettingsSubPage,
    chrome::kPowerSubPage,         chrome::kStylusSubPage,
    chrome::kWiFiSettingsSubPage,
};
#endif  // defined(OS_CHROMEOS)

}  // namespace

ChromeAutocompleteProviderClient::ChromeAutocompleteProviderClient(
    Profile* profile)
    : profile_(profile),
      scheme_classifier_(profile),
      url_consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedDataCollectionConsentHelper(
                  ProfileSyncServiceFactory::GetForProfile(profile_))),
      storage_partition_(nullptr) {
  if (OmniboxFieldTrial::IsPedalSuggestionsEnabled())
    pedal_provider_ = std::make_unique<OmniboxPedalProvider>(*this);
}

ChromeAutocompleteProviderClient::~ChromeAutocompleteProviderClient() {
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeAutocompleteProviderClient::GetURLLoaderFactory() {
  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

PrefService* ChromeAutocompleteProviderClient::GetPrefs() {
  return profile_->GetPrefs();
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
  // If Pedals are disabled, we should never get here to use the provider.
  DCHECK(OmniboxFieldTrial::IsPedalSuggestionsEnabled());
  DCHECK(pedal_provider_);
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

std::vector<base::string16> ChromeAutocompleteProviderClient::GetBuiltinURLs() {
  std::vector<std::string> chrome_builtins(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(chrome_builtins.begin(), chrome_builtins.end());

  std::vector<base::string16> builtins;

  for (auto i(chrome_builtins.begin()); i != chrome_builtins.end(); ++i)
    builtins.push_back(base::ASCIIToUTF16(*i));

#if !defined(OS_ANDROID)
  base::string16 settings(base::ASCIIToUTF16(chrome::kChromeUISettingsHost) +
                          base::ASCIIToUTF16("/"));
  for (size_t i = 0; i < base::size(kChromeSettingsSubPages); i++) {
    builtins.push_back(settings +
                       base::ASCIIToUTF16(kChromeSettingsSubPages[i]));
  }
#endif

#if defined(OS_CHROMEOS)
  // TODO(crbug/950007): Delete this after the settings split is complete since
  // the OS setting routes should not show up as an autocomplete suggestion in
  // browser.
  if (!base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)) {
    for (size_t i = 0; i < base::size(kChromeOSSettingsSubPages); i++) {
      builtins.push_back(settings +
                         base::ASCIIToUTF16(kChromeOSSettingsSubPages[i]));
    }
  }
#endif

  return builtins;
}

std::vector<base::string16>
ChromeAutocompleteProviderClient::GetBuiltinsToProvideAsUserTypes() {
  std::vector<base::string16> builtins_to_provide;
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

bool ChromeAutocompleteProviderClient::IsOffTheRecord() const {
  return profile_->IsOffTheRecord();
}

bool ChromeAutocompleteProviderClient::SearchSuggestEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool ChromeAutocompleteProviderClient::IsPersonalizedUrlDataCollectionActive()
    const {
  return url_consent_helper_->IsEnabled();
}

bool ChromeAutocompleteProviderClient::IsAuthenticated() const {
  const auto* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  return identity_manager && identity_manager->HasPrimaryAccount();
}

bool ChromeAutocompleteProviderClient::IsSyncActive() const {
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return sync && sync->IsSyncFeatureActive();
}

std::string ChromeAutocompleteProviderClient::ProfileUserName() const {
  return profile_->GetProfileUserName();
}

void ChromeAutocompleteProviderClient::Classify(
    const base::string16& text,
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
    const base::string16& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void ChromeAutocompleteProviderClient::PrefetchImage(const GURL& url) {
  // Note: Android uses different image fetching mechanism to avoid
  // penalty of copying byte buffers from C++ to Java.
#if !defined(OS_ANDROID)
  BitmapFetcherService* image_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  DCHECK(image_service);

  // TODO(jdonnelly, rhalavati): Create a helper function with Callback to
  // create annotation and pass it to image_service, merging the annotations
  // in omnibox_page_handler.cc, chrome_omnibox_client.cc,
  // and chrome_autocomplete_provider_client.cc.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_prefetch_image", R"(
        semantics {
          sender: "Omnibox"
          description:
            "Chromium provides answers in the suggestion list for certain "
            "queries that the user types in the omnibox. This request "
            "retrieves a small image (for example, an icon illustrating the "
            "current weather conditions) when this can add information to an "
            "answer."
          trigger:
            "Change of results for the query typed by the user in the "
            "omnibox."
          data:
            "The only data sent is the path to an image. No user data is "
            "included, although some might be inferrable (e.g. whether the "
            "weather is sunny or rainy in the user's current location) from "
            "the name of the image in the path."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "You can enable or disable this feature via 'Use a prediction "
            "service to help complete searches and URLs typed in the "
            "address bar.' in Chromium's settings under Advanced. The "
            "feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

  image_service->Prefetch(url, traffic_annotation);
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
    partition = content::BrowserContext::GetDefaultStoragePartition(profile_);
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
#if !defined(OS_ANDROID)
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  content::WebContents* active_tab = nullptr;
  if (active_browser)
    active_tab = active_browser->tab_strip_model()->GetActiveWebContents();
  const AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;
  const GURL stripped_url = AutocompleteMatch::GURLToStrippedGURL(
      url, *input, GetTemplateURLService(), base::string16());
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only look at same profile (and anonymity level).
    if (browser->profile()->IsSameProfileAndType(profile_)) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        if (web_contents != active_tab &&
            IsStrippedURLEqualToWebContentsURL(stripped_url, web_contents))
          return true;
      }
    }
  }
#endif  // !defined(OS_ANDROID)
  return false;
}

bool ChromeAutocompleteProviderClient::IsBrowserUpdateAvailable() const {
#if defined(OS_ANDROID)
  return false;
#else
  return UpgradeDetector::GetInstance()->is_upgrade_available();
#endif
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
             url1, *input, template_url_service, base::string16()) ==
         AutocompleteMatch::GURLToStrippedGURL(
             url2, *input, template_url_service, base::string16());
}

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
          base::string16());
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
    content::WebContents*)
    : content::WebContentsUserData<AutocompleteClientWebContentsUserData>() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutocompleteClientWebContentsUserData)

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

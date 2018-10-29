// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/contextual_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/identity_manager.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"
#endif

namespace {

#if !defined(OS_ANDROID)
// This list should be kept in sync with chrome/common/webui_url_constants.h.
// Only include useful sub-pages, confirmation alerts are not useful.
const char* const kChromeSettingsSubPages[] = {
    chrome::kAutofillSubPage,
    chrome::kClearBrowserDataSubPage,
    chrome::kContentSettingsSubPage,
    chrome::kLanguageOptionsSubPage,
    chrome::kPasswordManagerSubPage,
    chrome::kPaymentsSubPage,
    chrome::kResetProfileSettingsSubPage,
    chrome::kSearchEnginesSubPage,
    chrome::kSyncSetupSubPage,
#if defined(OS_CHROMEOS)
    chrome::kAccessibilitySubPage,
    chrome::kBluetoothSubPage,
    chrome::kDateTimeSubPage,
    chrome::kDisplaySubPage,
    chrome::kInternetSubPage,
    chrome::kPowerSubPage,
    chrome::kStylusSubPage,
#else
    chrome::kCreateProfileSubPage,
    chrome::kImportDataSubPage,
    chrome::kManageProfileSubPage,
    chrome::kPeopleSubPage,
#endif
};
#endif  // !defined(OS_ANDROID)

}  // namespace

ChromeAutocompleteProviderClient::ChromeAutocompleteProviderClient(
    Profile* profile)
    : profile_(profile),
      scheme_classifier_(profile),
      url_consent_helper_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedDataCollectionConsentHelper(
                  ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(
                      profile_))),
      storage_partition_(nullptr) {
  if (OmniboxFieldTrial::GetPedalSuggestionMode() !=
      OmniboxFieldTrial::PedalSuggestionMode::NONE)
    pedal_provider_ = std::make_unique<OmniboxPedalProvider>();
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

ContextualSuggestionsService*
ChromeAutocompleteProviderClient::GetContextualSuggestionsService(
    bool create_if_necessary) const {
  return ContextualSuggestionsServiceFactory::GetForProfile(
      profile_, create_if_necessary);
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
  DCHECK_NE(OmniboxFieldTrial::GetPedalSuggestionMode(),
            OmniboxFieldTrial::PedalSuggestionMode::NONE);
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
  return profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
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
  for (size_t i = 0; i < arraysize(kChromeSettingsSubPages); i++) {
    builtins.push_back(settings +
                       base::ASCIIToUTF16(kChromeSettingsSubPages[i]));
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

base::Time ChromeAutocompleteProviderClient::GetCurrentVisitTimestamp() const {
// The timestamp is currenly used only for contextual zero suggest suggestions
// on desktop. Consider updating this if this will be used for mobile services.
#if !defined(OS_ANDROID)
  const Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  if (!active_browser)
    return base::Time();

  const content::WebContents* active_tab =
      active_browser->tab_strip_model()->GetActiveWebContents();
  if (!active_tab)
    return base::Time();

  const content::NavigationEntry* navigation =
      active_tab->GetController().GetLastCommittedEntry();
  if (!navigation)
    return base::Time();

  return navigation->GetTimestamp();
#else
  return base::Time();
#endif  // !defined(OS_ANDROID)
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

bool ChromeAutocompleteProviderClient::IsUnifiedConsentGiven() const {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile_);
  return consent_service && consent_service->IsUnifiedConsentGiven();
}

bool ChromeAutocompleteProviderClient::IsSyncActive() const {
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetSyncServiceForBrowserContext(
          profile_);
  return sync && sync->IsSyncFeatureActive();
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
  BitmapFetcherService* image_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  DCHECK(image_service);

  // TODO(jdonnelly, rhalavati): Create a helper function with Callback to
  // create annotation and pass it to image_service, merging this annotation and
  // chrome/browser/ui/omnibox/chrome_omnibox_client.cc
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

void ChromeAutocompleteProviderClient::OnAutocompleteControllerResultReady(
    AutocompleteController* controller) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
      content::Source<AutocompleteController>(controller),
      content::NotificationService::NoDetails());
}

// TODO(crbug.com/46623): Maintain a map of URL->WebContents for fast look-up.
bool ChromeAutocompleteProviderClient::IsTabOpenWithURL(
    const GURL& url,
    const AutocompleteInput* input) {
#if !defined(OS_ANDROID)
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  content::WebContents* active_tab = nullptr;
  if (active_browser)
    active_tab = active_browser->tab_strip_model()->GetActiveWebContents();
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only look at same profile (and anonymity level).
    if (browser->profile()->IsSameProfile(profile_) &&
        browser->profile()->GetProfileType() == profile_->GetProfileType()) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        if (web_contents != active_tab &&
            StrippedURLsAreEqual(web_contents->GetLastCommittedURL(), url,
                                 input))
          return true;
      }
    }
  }
#endif  // !defined(OS_ANDROID)
  return false;
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

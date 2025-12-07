// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/autocomplete/tab_matcher_android.h"
#else
#include "chrome/browser/autocomplete/tab_matcher_desktop.h"
#endif

class Profile;
class TabMatcher;
class AutocompleteScoringModelService;
class OnDeviceTailModelService;
class AimEligibilityService;

namespace content {
class StoragePartition;
class WebContents;
}  // namespace content

namespace unified_consent {
class UrlKeyedDataCollectionConsentHelper;
}

class ChromeAutocompleteProviderClient : public AutocompleteProviderClient {
 public:
  explicit ChromeAutocompleteProviderClient(Profile* profile);
  using WebContentsGetter = base::RepeatingCallback<content::WebContents*()>;
  ChromeAutocompleteProviderClient(Profile* profile,
                                   WebContentsGetter web_contents_getter);

  ChromeAutocompleteProviderClient(const ChromeAutocompleteProviderClient&) =
      delete;
  ChromeAutocompleteProviderClient& operator=(
      const ChromeAutocompleteProviderClient&) = delete;

  ~ChromeAutocompleteProviderClient() override;

  // AutocompleteProviderClient:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  PrefService* GetPrefs() const override;
  PrefService* GetLocalState() override;
  std::string GetApplicationLocale() const override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  history::HistoryService* GetHistoryService() override;
  history_clusters::HistoryClustersService* GetHistoryClustersService()
      override;
  history_embeddings::HistoryEmbeddingsService* GetHistoryEmbeddingsService()
      override;
  scoped_refptr<history::TopSites> GetTopSites() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  history::URLDatabase* GetInMemoryDatabase() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;
  TemplateURLService* GetTemplateURLService() override;
  const TemplateURLService* GetTemplateURLService() const override;
  DocumentSuggestionsService* GetDocumentSuggestionsService() const override;
  RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const override;
  ZeroSuggestCacheService* GetZeroSuggestCacheService() override;
  const ZeroSuggestCacheService* GetZeroSuggestCacheService() const override;
  OmniboxPedalProvider* GetPedalProvider() const override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override;
  std::unique_ptr<KeywordExtensionsDelegate> GetKeywordExtensionsDelegate(
      KeywordProvider* keyword_provider) override;
  std::unique_ptr<UnscopedExtensionProviderDelegate>
  GetUnscopedExtensionProviderDelegate(
      UnscopedExtensionProvider* provider) override;
  std::string GetAcceptLanguages() const override;
  std::string GetEmbedderRepresentationOfAboutScheme() const override;
  std::vector<std::u16string> GetBuiltinURLs() override;
  std::vector<std::u16string> GetBuiltinsToProvideAsUserTypes() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  OmniboxTriggeredFeatureService* GetOmniboxTriggeredFeatureService()
      const override;
  signin::IdentityManager* GetIdentityManager() const override;
  AutocompleteScoringModelService* GetAutocompleteScoringModelService()
      const override;
  OnDeviceTailModelService* GetOnDeviceTailModelService() const override;
  ProviderStateService* GetProviderStateService() const override;
  tab_groups::TabGroupSyncService* GetTabGroupSyncService() const override;
  AimEligibilityService* GetAimEligibilityService() const override;

  bool IsOffTheRecord() const override;
  bool IsIncognitoProfile() const override;
  bool IsGuestSession() const override;
  bool SearchSuggestEnabled() const override;
  bool AllowDeletingBrowserHistory() const override;
  bool IsUrlDataCollectionActive() const override;
  bool IsPersonalizedUrlDataCollectionActive() const override;
  bool IsAuthenticated() const override;
  bool IsSyncActive() const override;
  std::string ProfileUserName() const override;
  void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) override;
  void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const std::u16string& term) override;
  void PrefetchImage(const GURL& url) override;
  void StartServiceWorker(const GURL& destination_url) override;
  const TabMatcher& GetTabMatcher() const override;
  bool IsIncognitoModeAvailable() const override;
  bool IsSharingHubAvailable() const override;
  bool IsHistoryEmbeddingsEnabled() const override;
  bool IsHistoryEmbeddingsSettingVisible() const override;
  bool IsLensEnabled() const override;
  bool AreLensEntrypointsVisible() const override;
  std::optional<bool> IsPagePaywalled() const override;
  bool ShouldSendContextualUrlSuggestParam() const override;
  bool ShouldSendPageTitleSuggestParam() const override;
  base::CallbackListSubscription GetLensSuggestInputsWhenReady(
      LensOverlaySuggestInputsCallback callback) const override;
  base::WeakPtr<AutocompleteProviderClient> GetWeakPtr() override;

  // OmniboxAction::Client:
  void OpenSharingHub() override;
  void NewIncognitoWindow() override;
  void OpenIncognitoClearBrowsingDataDialog() override;
  void CloseIncognitoWindows() override;
  void PromptPageTranslation() override;
  bool OpenJourneys(const std::string& query) override;
  void OpenLensOverlay(bool show) override;
  void IssueContextualSearchRequest(const GURL& destination_url,
                                    AutocompleteMatchType::Type match_type,
                                    bool is_zero_prefix_suggestion) override;

  // For testing.
  void set_storage_partition(content::StoragePartition* storage_partition) {
    storage_partition_ = storage_partition;
  }

 private:
  raw_ptr<Profile> profile_;
  // Callback to get the current WebContents. In the context of the Omnibox, it
  // returns the currently active tab's WebContents.
  // May be a null callback, must be checked before using.
  WebContentsGetter web_contents_getter_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  std::unique_ptr<OmniboxPedalProvider> pedal_provider_;
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      url_consent_helper_;
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      personalized_url_consent_helper_;
#if BUILDFLAG(IS_ANDROID)
  TabMatcherAndroid tab_matcher_;
#else
  TabMatcherDesktop tab_matcher_;
#endif

  // Injectable storage partitiion, used for testing.
  raw_ptr<content::StoragePartition> storage_partition_;

  std::unique_ptr<OmniboxTriggeredFeatureService>
      omnibox_triggered_feature_service_;

  base::WeakPtrFactory<ChromeAutocompleteProviderClient> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_PROVIDER_CLIENT_H_

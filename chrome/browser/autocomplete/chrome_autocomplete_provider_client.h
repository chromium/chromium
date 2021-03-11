// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"

#if defined(OS_ANDROID)
class TabAndroid;
class TabModel;
#endif  // defined(OS_ANDROID)

class Profile;

namespace content {
class StoragePartition;
class WebContents;
}

namespace unified_consent {
class UrlKeyedDataCollectionConsentHelper;
}

class ChromeAutocompleteProviderClient : public AutocompleteProviderClient {
 public:
  explicit ChromeAutocompleteProviderClient(Profile* profile);
  ~ChromeAutocompleteProviderClient() override;

  // AutocompleteProviderClient:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  PrefService* GetPrefs() override;
  PrefService* GetLocalState() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  history::HistoryService* GetHistoryService() override;
  scoped_refptr<history::TopSites> GetTopSites() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  history::URLDatabase* GetInMemoryDatabase() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;
  TemplateURLService* GetTemplateURLService() override;
  const TemplateURLService* GetTemplateURLService() const override;
  RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const override;
  DocumentSuggestionsService* GetDocumentSuggestionsService(
      bool create_if_necessary) const override;
  OmniboxPedalProvider* GetPedalProvider() const override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override;
  std::unique_ptr<KeywordExtensionsDelegate> GetKeywordExtensionsDelegate(
      KeywordProvider* keyword_provider) override;
  std::string GetAcceptLanguages() const override;
  std::string GetEmbedderRepresentationOfAboutScheme() const override;
  std::vector<std::u16string> GetBuiltinURLs() override;
  std::vector<std::u16string> GetBuiltinsToProvideAsUserTypes() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  query_tiles::TileService* GetQueryTileService() const override;
  OmniboxTriggeredFeatureService* GetOmniboxTriggeredFeatureService()
      const override;
  signin::IdentityManager* GetIdentityManager() const override;
  bool IsOffTheRecord() const override;
  bool SearchSuggestEnabled() const override;
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
  bool IsTabOpenWithURL(const GURL& url,
                        const AutocompleteInput* input) override;
  bool IsIncognitoModeAvailable() const override;
  void OnAutocompleteControllerResultReady(
      AutocompleteController* controller) override;

  // For testing.
  void set_storage_partition(content::StoragePartition* storage_partition) {
    storage_partition_ = storage_partition;
  }

  bool StrippedURLsAreEqual(const GURL& url1,
                            const GURL& url2,
                            const AutocompleteInput* input) const;

  // Performs a comparison of |stripped_url| to the stripped last committed
  // URL of |web_contents|, using the internal cache to avoid repeatedly
  // re-stripping the URL.
  bool IsStrippedURLEqualToWebContentsURL(const GURL& stripped_url,
                                          content::WebContents* web_contents);

#if defined(OS_ANDROID)
  // Returns a TabAndroid has opened same URL as |url|.
  TabAndroid* GetTabOpenWithURL(const GURL& url,
                                const AutocompleteInput* input);
  // Make a JNI call to get all the hidden tabs and non Custom tabs in
  // |tab_model|.
  std::vector<TabAndroid*> GetAllHiddenAndNonCCTTabs(
      const std::vector<TabModel*>& tab_models);
#endif  // defined(OS_ANDROID)

 private:
  Profile* profile_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  std::unique_ptr<OmniboxPedalProvider> pedal_provider_;
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      url_consent_helper_;

  // Injectable storage partitiion, used for testing.
  content::StoragePartition* storage_partition_;

  std::unique_ptr<OmniboxTriggeredFeatureService>
      omnibox_triggered_feature_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAutocompleteProviderClient);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_PROVIDER_CLIENT_H_

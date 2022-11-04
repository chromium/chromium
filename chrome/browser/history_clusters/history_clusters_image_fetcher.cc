// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_image_fetcher.h"

#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/history_clusters/core/config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace history_clusters {

namespace {

// Anonymous namespace factory based on LookalikeUrlServiceFactory.
class HistoryClustersImageServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static HistoryClustersImageFetcher* GetForProfile(Profile* profile) {
    return static_cast<HistoryClustersImageFetcher*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create=*/true));
  }
  static HistoryClustersImageServiceFactory* GetInstance() {
    return base::Singleton<HistoryClustersImageServiceFactory>::get();
  }

  HistoryClustersImageServiceFactory(
      const HistoryClustersImageServiceFactory&) = delete;
  HistoryClustersImageServiceFactory& operator=(
      const HistoryClustersImageServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      HistoryClustersImageServiceFactory>;

  // HistoryClustersImageServiceFactory:
  HistoryClustersImageServiceFactory()
      : ProfileKeyedServiceFactory("HistoryClustersImageServiceFactory") {
    DependsOn(SyncServiceFactory::GetInstance());
  }

  ~HistoryClustersImageServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new HistoryClustersImageFetcher(static_cast<Profile*>(profile));
  }
};

}  // namespace

// A one-time use object that uses Suggest to get an image URL corresponding
// to `search_query` and `entity_id`. This is a hacky temporary implementation,
// ideally this should be replaced by persisted Suggest-provided entities.
class HistoryClustersImageFetcher::SuggestEntityImageURLFetcher
    : public AutocompleteProviderListener {
 public:
  SuggestEntityImageURLFetcher(
      Profile* profile,
      ChromeAutocompleteProviderClient* autocomplete_provider_client,
      const std::u16string& search_query,
      const std::string& entity_id)
      : profile_(profile),
        search_query_(base::i18n::ToLower(search_query)),
        entity_id_(entity_id),
        // TODO(tommycli): Replace the `SearchProvider` usage with
        // `RemoteSuggestionsService`.
        search_provider_(
            base::MakeRefCounted<SearchProvider>(autocomplete_provider_client,
                                                 this)) {}
  SuggestEntityImageURLFetcher(const SuggestEntityImageURLFetcher&) = delete;

  // `callback` is called with the result.
  void Start(base::OnceCallback<void(const GURL&)> callback) {
    DCHECK(!callback_);
    callback_ = std::move(callback);

    AutocompleteInput autocomplete_input(
        search_query_, metrics::OmniboxEventProto::OTHER,
        ChromeAutocompleteSchemeClassifier(profile_));
    search_provider_->Start(autocomplete_input, /*minimal_changes=*/false);

    if (search_provider_->done())
      ProcessMatches();
  }

  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {
    if (search_provider_->done())
      ProcessMatches();
  }

 private:
  void ProcessMatches() {
    DCHECK(search_provider_->done());
    DCHECK(callback_);
    for (const auto& match : search_provider_->matches()) {
      // TODO(tommycli): `entity_id_` is not used yet, because it's always
      // empty right now.
      if (match.image_url.is_valid() &&
          base::i18n::ToLower(match.contents) == search_query_) {
        std::move(callback_).Run(match.image_url);
        break;
      }
    }

    // If we didn't find any matching images, still notify the caller.
    if (!callback_.is_null())
      std::move(callback_).Run(GURL());
  }

  const raw_ptr<Profile> profile_;

  // The search query and entity ID we are searching for.
  const std::u16string search_query_;
  const std::string entity_id_;

  // The result callback to be called once we get the answer.
  base::OnceCallback<void(const GURL&)> callback_;

  // Our internally owned one-time-use search provider.
  scoped_refptr<AutocompleteProvider> search_provider_;
};

HistoryClustersImageFetcher::HistoryClustersImageFetcher(Profile* profile)
    : profile_(profile),
      autocomplete_provider_client_(profile),
      url_consent_helper_(unified_consent::UrlKeyedDataCollectionConsentHelper::
                              NewPersonalizedDataCollectionConsentHelper(
                                  SyncServiceFactory::GetForProfile(profile))) {
}

HistoryClustersImageFetcher::~HistoryClustersImageFetcher() = default;

// static
HistoryClustersImageFetcher* HistoryClustersImageFetcher::Get(
    Profile* profile) {
  return HistoryClustersImageServiceFactory::GetForProfile(profile);
}

bool HistoryClustersImageFetcher::FetchImageFor(
    const std::u16string& search_query,
    const std::string& entity_id,
    ResultCallback callback) {
  if (!GetConfig().images)
    return false;

  if (!url_consent_helper_ || !url_consent_helper_->IsEnabled())
    return false;

  auto fetcher = std::make_unique<SuggestEntityImageURLFetcher>(
      profile_, &autocomplete_provider_client_, search_query, entity_id);

  // Use a raw pointer temporary so we can give ownership of the unique_ptr to
  // the callback and have a well defined SuggestEntityImageURLFetcher lifetime.
  auto* fetcher_raw_ptr = fetcher.get();
  fetcher_raw_ptr->Start(base::BindOnce(
      &HistoryClustersImageFetcher::OnImageFetched, weak_factory_.GetWeakPtr(),
      std::move(fetcher), std::move(callback)));
  return true;
}

void HistoryClustersImageFetcher::OnImageFetched(
    std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
    ResultCallback callback,
    const GURL& image_url) {
  std::move(callback).Run(image_url);

  // `fetcher` is owned by this method and will be deleted now.
}

}  // namespace history_clusters

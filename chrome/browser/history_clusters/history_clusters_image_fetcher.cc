// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_image_fetcher.h"

#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/history_clusters/core/config.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search_engines/template_url.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

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
class HistoryClustersImageFetcher::SuggestEntityImageURLFetcher {
 public:
  SuggestEntityImageURLFetcher(
      Profile* profile,
      ChromeAutocompleteProviderClient* autocomplete_provider_client,
      const std::u16string& search_query,
      const std::string& entity_id)
      : profile_(profile),
        autocomplete_provider_client_(autocomplete_provider_client),
        search_query_(base::i18n::ToLower(search_query)),
        entity_id_(entity_id) {
    DCHECK(profile);
    DCHECK(autocomplete_provider_client);
  }
  SuggestEntityImageURLFetcher(const SuggestEntityImageURLFetcher&) = delete;

  // `callback` is called with the result.
  void Start(base::OnceCallback<void(const GURL&)> callback) {
    DCHECK(!callback_);
    callback_ = std::move(callback);

    TemplateURLRef::SearchTermsArgs search_terms_args;
    // TODO(tommycli): Change OTHER to a Journeys-specific page classification.
    search_terms_args.page_classification = metrics::OmniboxEventProto::OTHER;
    search_terms_args.search_terms = search_query_;

    loader_ =
        autocomplete_provider_client_
            ->GetRemoteSuggestionsService(/*create_if_necessary=*/true)
            ->StartSuggestionsRequest(
                search_terms_args,
                autocomplete_provider_client_->GetTemplateURLService(),
                base::BindOnce(&SuggestEntityImageURLFetcher::OnURLLoadComplete,
                               weak_factory_.GetWeakPtr()));
  }

 private:
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body) {
    DCHECK_EQ(loader_.get(), source);
    const bool response_received =
        response_body && source->NetError() == net::OK &&
        (source->ResponseInfo() && source->ResponseInfo()->headers &&
         source->ResponseInfo()->headers->response_code() == 200);
    if (!response_received) {
      return std::move(callback_).Run(GURL());
    }

    std::string response_json = SearchSuggestionParser::ExtractJsonData(
        source, std::move(response_body));
    if (response_json.empty()) {
      return std::move(callback_).Run(GURL());
    }

    auto response_data =
        SearchSuggestionParser::DeserializeJsonData(response_json);
    if (!response_data) {
      return std::move(callback_).Run(GURL());
    }

    // TODO(tommycli): Change OTHER to a Journeys-specific page classification.
    AutocompleteInput input(
        search_query_, metrics::OmniboxEventProto::OTHER,
        autocomplete_provider_client_->GetSchemeClassifier());
    SearchSuggestionParser::Results results;
    if (!SearchSuggestionParser::ParseSuggestResults(
            *response_data, input,
            autocomplete_provider_client_->GetSchemeClassifier(),
            /*default_result_relevance=*/100,
            /*is_keyword_result=*/false, &results)) {
      return std::move(callback_).Run(GURL());
    }

    for (const auto& result : results.suggest_results) {
      // TODO(tommycli): `entity_id_` is not used yet, because it's always
      // empty right now.
      if (result.image_url().is_valid() &&
          base::i18n::ToLower(result.match_contents()) == search_query_) {
        return std::move(callback_).Run(result.image_url());
      }
    }

    // If we didn't find any matching images, still notify the caller.
    if (!callback_.is_null())
      std::move(callback_).Run(GURL());
  }

  const raw_ptr<Profile> profile_;
  const raw_ptr<AutocompleteProviderClient> autocomplete_provider_client_;

  // The search query and entity ID we are searching for.
  const std::u16string search_query_;
  const std::string entity_id_;

  // The result callback to be called once we get the answer.
  base::OnceCallback<void(const GURL&)> callback_;

  // The URL loader used to get the suggestions.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  base::WeakPtrFactory<SuggestEntityImageURLFetcher> weak_factory_{this};
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

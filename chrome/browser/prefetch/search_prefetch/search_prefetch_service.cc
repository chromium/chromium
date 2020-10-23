// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/prefetched_response_container.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/origin.h"

SearchPrefetchService::PrefetchRequest::PrefetchRequest(
    const GURL& prefetch_url,
    base::OnceClosure report_error_callback)
    : prefetch_url_(prefetch_url),
      report_error_callback_(std::move(report_error_callback)) {}

SearchPrefetchService::PrefetchRequest::~PrefetchRequest() = default;

void SearchPrefetchService::PrefetchRequest::StartPrefetchRequest(
    Profile* profile) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("search_prefetch_service", R"(
        semantics {
          sender: "Search Prefetch Service"
          description:
            "Prefetches search results page (HTML) based on omnibox hints "
            "provided by the user's default search engine. This allows the "
            "prefetched content to be served when the user navigates to the "
            "omnibox hint."
          trigger:
            "User typing in the omnibox and the default search provider "
            "indicates the provided omnibox hint entry is likely to be "
            "navigated which would result in loading a search results page for "
            "that hint."
          data: "Credentials if user is signed in."
          destination: OTHER
          destination_other: "The user's default search engine."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature by opting out of 'Preload pages "
            "for faster browsing and searching'"
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            NetworkPredictionOptions {
              NetworkPredictionOptions: 2
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->load_flags |= net::LOAD_PREFETCH;
  resource_request->url = prefetch_url_;
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  variations::AppendVariationsHeaderUnknownSignedIn(
      prefetch_url_, variations::InIncognito::kNo, resource_request.get());
  resource_request->headers.SetHeader(content::kCorsExemptPurposeHeaderName,
                                      "prefetch");
  // TODO(ryansturm): Find other headers that may need to be set.
  // https://crbug.com/1138648

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);

  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();

  simple_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&SearchPrefetchService::PrefetchRequest::LoadDone,
                     base::Unretained(this)),
      1024 * 1024);
}

void SearchPrefetchService::PrefetchRequest::LoadDone(
    std::unique_ptr<std::string> response_body) {
  bool success = simple_loader_->NetError() == net::OK;
  int response_code = 0;

  // TODO(ryansturm): Handle these errors more robustly by reporting them to the
  // service. We need to prevent prefetches for x amount of time based on the
  // error. https://crbug.com/1138641
  if (!success || response_body->empty()) {
    current_status_ = SearchPrefetchStatus::kRequestFailed;
    std::move(report_error_callback_).Run();
    return;
  }
  if (simple_loader_->ResponseInfo() && simple_loader_->ResponseInfo()->headers)
    response_code = simple_loader_->ResponseInfo()->headers->response_code();
  if (response_code != net::HTTP_OK) {
    current_status_ = SearchPrefetchStatus::kRequestFailed;
    std::move(report_error_callback_).Run();
    return;
  }
  current_status_ = SearchPrefetchStatus::kSuccessfullyCompleted;

  prefetch_response_container_ = std::make_unique<PrefetchedResponseContainer>(
      simple_loader_->ResponseInfo()->Clone(), std::move(response_body));

  simple_loader_.reset();
}

std::unique_ptr<PrefetchedResponseContainer>
SearchPrefetchService::PrefetchRequest::TakePrefetchResponse() {
  return std::move(prefetch_response_container_);
}

SearchPrefetchService::SearchPrefetchService(Profile* profile)
    : profile_(profile) {
  DCHECK(!profile_->IsOffTheRecord());
}

SearchPrefetchService::~SearchPrefetchService() = default;

bool SearchPrefetchService::MaybePrefetchURL(const GURL& url) {
  if (!SearchPrefetchServicePrefetchingIsEnabled())
    return false;

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service)
    return false;
  base::string16 search_terms;

  // Extract the terms directly to make sure this string will match the URL
  // interception string logic.
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.size() == 0)
    return false;

  if (last_error_time_ticks_ + SearchPrefetchErrorBackoffDuration() >
      base::TimeTicks::Now()) {
    return false;
  }

  if (prefetches_.size() >= SearchPrefetchMaxAttemptsPerCachingDuration())
    return false;

  // Don't prefetch the same search terms twice within the expiry duration.
  if (prefetches_.find(search_terms) != prefetches_.end()) {
    return false;
  }

  prefetches_.emplace(
      search_terms, std::make_unique<PrefetchRequest>(
                        url, base::BindOnce(&SearchPrefetchService::ReportError,
                                            base::Unretained(this))));
  prefetches_[search_terms]->StartPrefetchRequest(profile_);
  prefetch_expiry_timers_.emplace(search_terms,
                                  std::make_unique<base::OneShotTimer>());
  prefetch_expiry_timers_[search_terms]->Start(
      FROM_HERE, SearchPrefetchCachingLimit(),
      base::BindOnce(&SearchPrefetchService::DeletePrefetch,
                     base::Unretained(this), search_terms));
  return true;
}

base::Optional<SearchPrefetchStatus>
SearchPrefetchService::GetSearchPrefetchStatusForTesting(
    base::string16 search_terms) {
  if (prefetches_.find(search_terms) == prefetches_.end())
    return base::nullopt;
  return prefetches_[search_terms]->current_status();
}

std::unique_ptr<PrefetchedResponseContainer>
SearchPrefetchService::TakePrefetchResponse(const GURL& url) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service)
    return nullptr;

  base::string16 search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);

  if (search_terms.length() == 0) {
    return nullptr;
  }

  const auto& iter = prefetches_.find(search_terms);

  if (iter == prefetches_.end()) {
    return nullptr;
  }

  // Verify that the URL is the same origin as the prefetch URL. While other
  // checks should address this by clearing prefetches on user changes to
  // default search, it is paramount to never serve content from one origin to
  // another.
  if (url::Origin::Create(url) !=
      url::Origin::Create(iter->second->prefetch_url())) {
    return nullptr;
  }

  if (iter->second->current_status() !=
      SearchPrefetchStatus::kSuccessfullyCompleted) {
    return nullptr;
  }

  std::unique_ptr<PrefetchedResponseContainer> response =
      iter->second->TakePrefetchResponse();

  // TODO(ryansturm): For metrics reporting, the prefetch request data should be
  // moved to the correct tab helper object, for now, the object can be deleted
  // entirely. Alternatively, the object can remain here with a new timeout in
  // a set of currently being served requests.
  DeletePrefetch(search_terms);

  return response;
}

void SearchPrefetchService::DeletePrefetch(base::string16 search_terms) {
  DCHECK(prefetches_.find(search_terms) != prefetches_.end());
  DCHECK(prefetch_expiry_timers_.find(search_terms) !=
         prefetch_expiry_timers_.end());

  prefetches_.erase(search_terms);
  prefetch_expiry_timers_.erase(search_terms);
}

void SearchPrefetchService::ReportError() {
  last_error_time_ticks_ = base::TimeTicks::Now();
}

void SearchPrefetchService::OnResultChanged(
    AutocompleteController* controller) {
  const auto& result = controller->result();
  const auto* default_match = result.default_match();

  // One arm of the experiment only prefetches the top match when it is default.
  if (SearchPrefetchOnlyFetchDefaultMatch()) {
    if (default_match && BaseSearchProvider::ShouldPrefetch(*default_match)) {
      MaybePrefetchURL(default_match->destination_url);
    }
    return;
  }

  for (const auto& match : result) {
    if (BaseSearchProvider::ShouldPrefetch(match)) {
      MaybePrefetchURL(match.destination_url);
    }
  }
}

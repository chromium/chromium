// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/prefetched_response_container.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
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

SearchPrefetchService::PrefetchRequest::PrefetchRequest(
    const GURL& prefetch_url)
    : prefetch_url_(prefetch_url) {}

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
    return;
  }
  if (simple_loader_->ResponseInfo() && simple_loader_->ResponseInfo()->headers)
    response_code = simple_loader_->ResponseInfo()->headers->response_code();
  if (response_code != net::HTTP_OK) {
    current_status_ = SearchPrefetchStatus::kRequestFailed;
    return;
  }
  current_status_ = SearchPrefetchStatus::kSuccessfullyCompleted;

  prefetch_response_container_ = std::make_unique<PrefetchedResponseContainer>(
      simple_loader_->ResponseInfo()->Clone(), std::move(response_body));

  simple_loader_.reset();
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

  // Don't prefetch the same search terms twice within the expiry duration.
  if (prefetches_.find(search_terms) != prefetches_.end()) {
    return false;
  }

  prefetches_.emplace(search_terms, std::make_unique<PrefetchRequest>(url));
  prefetches_[search_terms]->StartPrefetchRequest(profile_);
  return true;
  // TODO(ryansturm): Expire entries after 60 seconds. https://crbug.com/1138639
}

base::Optional<SearchPrefetchStatus>
SearchPrefetchService::GetSearchPrefetchStatusForTesting(
    base::string16 search_terms) {
  if (prefetches_.find(search_terms) == prefetches_.end())
    return base::nullopt;
  return prefetches_[search_terms]->current_status();
}

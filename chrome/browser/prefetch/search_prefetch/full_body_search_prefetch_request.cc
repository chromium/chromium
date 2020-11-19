// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/full_body_search_prefetch_request.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/prefetched_response_container.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_from_string_url_loader.h"
#include "chrome/browser/profiles/profile.h"
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

FullBodySearchPrefetchRequest::FullBodySearchPrefetchRequest(
    const GURL& prefetch_url,
    base::OnceClosure report_error_callback)
    : BaseSearchPrefetchRequest(prefetch_url,
                                std::move(report_error_callback)) {}

FullBodySearchPrefetchRequest::~FullBodySearchPrefetchRequest() = default;

void FullBodySearchPrefetchRequest::StartPrefetchRequestInternal(
    Profile* profile,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation) {
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    network_traffic_annotation);

  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();

  simple_loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&FullBodySearchPrefetchRequest::LoadDone,
                     base::Unretained(this)),
      1024 * 1024);
}

void FullBodySearchPrefetchRequest::LoadDone(
    std::unique_ptr<std::string> response_body) {
  DCHECK(!report_error_callback_.is_null());
  bool success = simple_loader_->NetError() == net::OK;
  // TODO(ryansturm): Handle these errors more robustly by reporting them to the
  // service. We need to prevent prefetches for x amount of time based on the
  // error. https://crbug.com/1138641
  if (!success || response_body->empty()) {
    current_status_ = SearchPrefetchStatus::kRequestFailed;
    std::move(report_error_callback_).Run();
    return;
  }
  if (!simple_loader_->ResponseInfo() ||
      !simple_loader_->ResponseInfo()->headers ||
      simple_loader_->ResponseInfo()->headers->response_code() !=
          net::HTTP_OK) {
    current_status_ = SearchPrefetchStatus::kRequestFailed;
    std::move(report_error_callback_).Run();
    return;
  }
  current_status_ = SearchPrefetchStatus::kSuccessfullyCompleted;

  prefetch_response_container_ = std::make_unique<PrefetchedResponseContainer>(
      simple_loader_->ResponseInfo()->Clone(), std::move(response_body));

  simple_loader_.reset();
}

std::unique_ptr<SearchPrefetchURLLoader>
FullBodySearchPrefetchRequest::TakeSearchPrefetchURLLoader() {
  return std::make_unique<SearchPrefetchFromStringURLLoader>(
      std::move(prefetch_response_container_));
}

void FullBodySearchPrefetchRequest::CancelPrefetch() {
  DCHECK_EQ(current_status_, SearchPrefetchStatus::kInFlight);
  current_status_ = SearchPrefetchStatus::kRequestCancelled;

  simple_loader_.reset();
}

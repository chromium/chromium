// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_bridge.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "chrome/browser/android/explore_sites/url_util.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace explore_sites {

namespace {

// Content type needed in order to communicate with the server in binary
// proto format.
const char kRequestContentType[] = "application/x-protobuf";
const char kRequestMethod[] = "GET";

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("explore_sites", R"(
          semantics {
            sender: "Explore Sites"
            description:
              "Downloads a catalog of categories and sites for the purposes of "
              "exploring the Web."
            trigger:
              "Periodically scheduled for update in the background and also "
              "triggered by New Tab Page UI."
            data:
              "Proto data comprising interesting site and category information."
              " No user information is sent."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
          })");

}  // namespace

const net::BackoffEntry::Policy
    ExploreSitesFetcher::kImmediateFetchBackoffPolicy = {
        0,         // Number of initial errors to ignore without backoff.
        1 * 1000,  // Initial delay for backoff in ms: 1 second.
        2,         // Factor to multiply for exponential backoff.
        0,         // Fuzzing percentage.
        4 * 1000,  // Maximum time to delay requests in ms: 4 seconds.
        -1,        // Don't discard entry even if unused.
        false      // Don't use initial delay unless the last was an error.
};
const int ExploreSitesFetcher::kMaxFailureCountForImmediateFetch = 3;

const net::BackoffEntry::Policy
    ExploreSitesFetcher::kBackgroundFetchBackoffPolicy = {
        0,           // Number of initial errors to ignore without backoff.
        5 * 1000,    // Initial delay for backoff in ms: 5 seconds.
        2,           // Factor to multiply for exponential backoff.
        0,           // Fuzzing percentage.
        320 * 1000,  // Maximum time to delay requests in ms: 320 seconds.
        -1,          // Don't discard entry even if unused.
        false        // Don't use initial delay unless the last was an error.
};
const int ExploreSitesFetcher::kMaxFailureCountForBackgroundFetch = 7;

// static
std::unique_ptr<ExploreSitesFetcher> ExploreSitesFetcher::CreateForGetCatalog(
    bool is_immediate_fetch,
    const std::string& catalog_version,
    const std::string& accept_languages,
    const std::string& country_code,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    Callback callback) {
  GURL url = GetCatalogURL();
  return base::WrapUnique(new ExploreSitesFetcher(
      is_immediate_fetch, url, catalog_version, accept_languages, country_code,
      loader_factory, std::move(callback)));
}

ExploreSitesFetcher::ExploreSitesFetcher(
    bool is_immediate_fetch,
    const GURL& url,
    const std::string& catalog_version,
    const std::string& accept_languages,
    const std::string& country_code,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    Callback callback)
    : is_immediate_fetch_(is_immediate_fetch),
      accept_languages_(accept_languages),
      country_code_(country_code),
      catalog_version_(catalog_version),
      url_(url),
      device_delegate_(std::make_unique<DeviceDelegate>()),
      callback_(std::move(callback)),
      url_loader_factory_(loader_factory) {
  base::Version version = version_info::GetVersion();
  std::string channel_name = chrome::GetChannelName();
  client_version_ = base::StringPrintf("%d.%d.%d.%s.chrome",
                                       version.components()[0],  // Major
                                       version.components()[2],  // Build
                                       version.components()[3],  // Patch
                                       channel_name.c_str());
  UpdateBackoffEntry();
}

ExploreSitesFetcher::~ExploreSitesFetcher() {}

void ExploreSitesFetcher::Start() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  GURL request_url =
      net::AppendOrReplaceQueryParameter(url_, "country_code", country_code_);
  request_url = net::AppendOrReplaceQueryParameter(request_url, "version_token",
                                                   catalog_version_);
  resource_request->url = request_url;
  resource_request->method = kRequestMethod;
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;
  std::string api_key = is_stable_channel ? google_apis::GetAPIKey()
                                          : google_apis::GetNonStableAPIKey();
  resource_request->headers.SetHeader("x-goog-api-key", api_key);
  resource_request->headers.SetHeader("X-Client-Version", client_version_);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kRequestContentType);
  std::string scale_factor =
      std::to_string(device_delegate_->GetScaleFactorFromDevice());
  resource_request->headers.SetHeader("X-Device-Scale-Factor", scale_factor);

  if (!accept_languages_.empty()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAcceptLanguage, accept_languages_);
  }

  // Get field trial value, if any.
  std::string tag = base::GetFieldTrialParamValueByFeature(
      chrome::android::kExploreSites,
      chrome::android::explore_sites::
          kExploreSitesHeadersExperimentParameterName);

  if (!tag.empty()) {
    resource_request->headers.SetHeader("X-Goog-Chrome-Experiment-Tag", tag);
  }

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ExploreSitesFetcher::OnSimpleLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

float ExploreSitesFetcher::DeviceDelegate::GetScaleFactorFromDevice() {
  return ExploreSitesBridge::GetScaleFactorFromDevice();
}

void ExploreSitesFetcher::SetDeviceDelegateForTest(
    std::unique_ptr<ExploreSitesFetcher::DeviceDelegate> device_delegate) {
  device_delegate_ = std::move(device_delegate);
}

void ExploreSitesFetcher::RestartAsImmediateFetchIfNotYet() {
  if (is_immediate_fetch_)
    return;
  is_immediate_fetch_ = true;

  UpdateBackoffEntry();
  url_loader_.reset();
  Start();
}

void ExploreSitesFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  ExploreSitesRequestStatus status = HandleResponseCode();

  if (response_body && response_body->empty()) {
    DVLOG(1) << "Failed to get response or empty response";
    status = ExploreSitesRequestStatus::kFailure;
  }

  if (status == ExploreSitesRequestStatus::kFailure &&
      !disable_retry_for_testing_) {
    RetryWithBackoff();
    return;
  }

  std::move(callback_).Run(status, std::move(response_body));
}

ExploreSitesRequestStatus ExploreSitesFetcher::HandleResponseCode() {
  int response_code = -1;
  int net_error = url_loader_->NetError();
  base::UmaHistogramSparse("ExploreSites.FetcherNetErrorCode", -net_error);

  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  if (response_code == -1) {
    DVLOG(1) << "Net error: " << net_error;
    return (net_error == net::ERR_BLOCKED_BY_ADMINISTRATOR)
               ? ExploreSitesRequestStatus::kShouldSuspendBlockedByAdministrator
               : ExploreSitesRequestStatus::kFailure;
  }

  base::UmaHistogramSparse("ExploreSites.FetcherHttpResponseCode",
                           response_code);

  if (response_code < 200 || response_code > 299) {
    DVLOG(1) << "HTTP status: " << response_code;
    switch (response_code) {
      case net::HTTP_BAD_REQUEST:
        return ExploreSitesRequestStatus::kShouldSuspendBadRequest;
      default:
        return ExploreSitesRequestStatus::kFailure;
    }
  }

  return ExploreSitesRequestStatus::kSuccess;
}

void ExploreSitesFetcher::RetryWithBackoff() {
  backoff_entry_->InformOfRequest(false);

  if (backoff_entry_->failure_count() >= max_failure_count_) {
    std::move(callback_).Run(ExploreSitesRequestStatus::kFailure,
                             std::unique_ptr<std::string>());
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExploreSitesFetcher::Start, weak_factory_.GetWeakPtr()),
      backoff_entry_->GetTimeUntilRelease());
}

void ExploreSitesFetcher::UpdateBackoffEntry() {
  backoff_entry_ = std::make_unique<net::BackoffEntry>(
      is_immediate_fetch_ ? &kImmediateFetchBackoffPolicy
                          : &kBackgroundFetchBackoffPolicy);
  max_failure_count_ = is_immediate_fetch_ ? kMaxFailureCountForImmediateFetch
                                           : kMaxFailureCountForBackgroundFetch;
}

}  // namespace explore_sites

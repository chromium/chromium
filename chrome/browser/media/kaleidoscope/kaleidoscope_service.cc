// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_service.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_service_factory.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace kaleidoscope {

namespace {

const char kRequestContentType[] = "application/x-protobuf";

const char kCollectionsURLFormat[] = "/v1/collections";

class GetCollectionsRequest {
 public:
  GetCollectionsRequest(
      media::mojom::CredentialsPtr credentials,
      const std::string& gaia_id,
      const std::string& request_b64,
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory,
      base::Clock* clock,
      base::OnceCallback<void(const std::string&)> callback)
      : gaia_id_(gaia_id), clock_(clock), start_time_(clock->Now()) {
    const auto base_url =
        GetGoogleAPIBaseURL(*base::CommandLine::ForCurrentProcess());

    GURL::Replacements replacements;
    replacements.SetPathStr(kCollectionsURLFormat);

    std::string request_body;
    base::Base64Decode(request_b64, &request_body);

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("kaleidoscope_service", R"(
        semantics {
          sender: "Kaleidoscope Service"
          description:
            "Kaleidoscope fetches media recommendations from Google and "
            "displays them on the New Tab Page."
          trigger:
            "Opening the New Tab Page after having not opened the New Tab Page "
            "for more than 24 hours. Opening the New Tab Page after having "
            "signed in/signed out to a different user account. "
          data: "Google account login."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
             "The feature is enabled by default. The user can disable "
             "individual media feeds. The feature does not operate in "
             "incognito mode."
          policy_exception_justification:
             "Not implemented."
        })");
    auto resource_request = std::make_unique<::network::ResourceRequest>();
    resource_request->url = base_url.ReplaceComponents(replacements);
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    if (credentials->access_token.has_value()) {
      resource_request->headers.SetHeader(
          net::HttpRequestHeaders::kAuthorization,
          base::StrCat({"Bearer ", *credentials->access_token}));
    }

    if (!credentials->api_key.empty()) {
      resource_request->headers.SetHeader("X-Goog-Api-Key",
                                          credentials->api_key);
    }

    resource_request->headers.SetHeader("X-Goog-Encode-Response-If-Executable",
                                        "base64");

    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kRequestContentType);
    pending_request_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    pending_request_->SetAllowHttpErrorResults(true);
    pending_request_->AttachStringForUpload(request_body, kRequestContentType);
    pending_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindOnce(&GetCollectionsRequest::OnDataLoaded,
                       base::Unretained(this), std::move(callback)));
  }

  ~GetCollectionsRequest() = default;

  std::string gaia_id() const { return gaia_id_; }

  network::SimpleURLLoader& url_loader() const {
    return *pending_request_.get();
  }

  bool has_failed() const {
    return url_loader().NetError() != net::OK ||
           response_code() != net::HTTP_OK;
  }

  bool not_available() const {
    return url_loader().NetError() == net::OK &&
           response_code() == net::HTTP_FORBIDDEN;
  }

 private:
  void OnDataLoaded(base::OnceCallback<void(const std::string&)> callback,
                    std::unique_ptr<std::string> data) {
    base::TimeDelta time_taken = clock_->Now() - start_time_;
    base::UmaHistogramTimes(
        KaleidoscopeService::kNTPModuleServerFetchTimeHistogramName,
        time_taken);

    if (data) {
      std::move(callback).Run(*data);
    } else {
      std::move(callback).Run(std::string());
    }
  }

  int response_code() const {
    if (url_loader().ResponseInfo() && url_loader().ResponseInfo()->headers) {
      return url_loader().ResponseInfo()->headers->response_code();
    }

    return 0;
  }

  std::string const gaia_id_;
  base::Clock* const clock_;
  base::Time const start_time_;

  std::unique_ptr<::network::SimpleURLLoader> pending_request_;
};

}  // namespace

const char KaleidoscopeService::kNTPModuleCacheHitHistogramName[] =
    "Media.Kaleidoscope.NewTabPage.CacheHitWhenForced";

const char KaleidoscopeService::kNTPModuleServerFetchTimeHistogramName[] =
    "Media.Kaleidoscope.NewTabPage.ServerFetchTime";

KaleidoscopeService::KaleidoscopeService(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  DCHECK(!profile->IsOffTheRecord());
  DCHECK(clock_);
}

// static
KaleidoscopeService* KaleidoscopeService::Get(Profile* profile) {
  return KaleidoscopeServiceFactory::GetForProfile(profile);
}

KaleidoscopeService::~KaleidoscopeService() = default;

bool KaleidoscopeService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kKaleidoscope);
}

void KaleidoscopeService::GetCollections(
    media::mojom::CredentialsPtr credentials,
    const std::string& gaia_id,
    const std::string& request,
    GetCollectionsCallback callback) {
  // If the GAIA id has changed then reset the request if there is one inflight.
  if (request_ && request_->gaia_id() != gaia_id) {
    request_.reset();
  }

  // Check Media History if we have any cached kaleidoscope data.
  media_history::MediaHistoryKeyedService::Get(profile_)->GetKaleidoscopeData(
      gaia_id,
      base::BindOnce(&KaleidoscopeService::OnGotCachedData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(credentials),
                     gaia_id, request, std::move(callback)));
}

void KaleidoscopeService::SetCollectionsForTesting(
    const std::string& collections) {
  media_history::MediaHistoryKeyedService::Get(profile_)->SetKaleidoscopeData(
      media::mojom::GetCollectionsResponse::New(
          collections, media::mojom::GetCollectionsResult::kSuccess),
      "");
}

bool KaleidoscopeService::ShouldShowFirstRunExperience() {
  // If the flag for forcing the first run experience to show is set, then just
  // show it.
  if (base::FeatureList::IsEnabled(
          media::kKaleidoscopeForceShowFirstRunExperience)) {
    return true;
  }

  // Otherwise, check to see if the user has already completed the latest first
  // run experience.
  auto* prefs = profile_->GetPrefs();
  if (!prefs)
    return true;

  // If the pref is unset or lower than the current version, then we haven't
  // shown the current first run experience before and we should show it now.
  const base::Value* pref = prefs->GetUserPrefValue(
      kaleidoscope::prefs::kKaleidoscopeFirstRunCompleted);
  if (!pref || pref->GetInt() < kKaleidoscopeFirstRunLatestVersion)
    return true;

  // Otherwise, we have shown it and don't need to.
  return false;
}

void KaleidoscopeService::OnGotCachedData(
    media::mojom::CredentialsPtr credentials,
    const std::string& gaia_id,
    const std::string& request,
    GetCollectionsCallback callback,
    media::mojom::GetCollectionsResponsePtr cached) {
  // If we got cached data then return that.
  if (cached) {
    if (base::FeatureList::IsEnabled(media::kKaleidoscopeModuleCacheOnly)) {
      base::UmaHistogramEnumeration(kNTPModuleCacheHitHistogramName,
                                    CacheHitResult::kCacheHit);
    }

    std::move(callback).Run(std::move(cached));
    return;
  }

  // If the module is set to "cache only" then we will return an empty response
  // and fire the request in the background. The next time the user opens the
  // NTP they will see the recommendations.
  if (base::FeatureList::IsEnabled(media::kKaleidoscopeModuleCacheOnly)) {
    base::UmaHistogramEnumeration(kNTPModuleCacheHitHistogramName,
                                  CacheHitResult::kCacheMiss);

    std::move(callback).Run(media::mojom::GetCollectionsResponse::New(
        "", media::mojom::GetCollectionsResult::kFailed));
  } else {
    // Add the callback.
    pending_callbacks_.push_back(std::move(callback));
  }

  // Create the request.
  if (!request_) {
    request_ = std::make_unique<GetCollectionsRequest>(
        std::move(credentials), gaia_id, request,
        GetURLLoaderFactoryForFetcher(), clock_,
        base::BindOnce(&KaleidoscopeService::OnURLFetchComplete,
                       base::Unretained(this), gaia_id));
  }
}

void KaleidoscopeService::OnURLFetchComplete(const std::string& gaia_id,
                                             const std::string& data) {
  auto response = media::mojom::GetCollectionsResponse::New();
  if (request_->not_available()) {
    response->result = media::mojom::GetCollectionsResult::kNotAvailable;
  } else if (request_->has_failed()) {
    response->result = media::mojom::GetCollectionsResult::kFailed;
  } else if (ShouldShowFirstRunExperience()) {
    // If we should show the first run experience then we should send a special
    // "first run" response which will trigger the module to display the first
    // run promo message.
    response->result = media::mojom::GetCollectionsResult::kFirstRun;
  } else {
    response->result = media::mojom::GetCollectionsResult::kSuccess;
    response->response = data;
  }

  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run(response.Clone());
  }

  pending_callbacks_.clear();
  request_.reset();

  // If the request did not fail then we should save it in the cache so we avoid
  // hitting the server later. If the response was that Kaleidoscope is not
  // available to the user then that is cacheable too.
  if (response->result != media::mojom::GetCollectionsResult::kFailed) {
    media_history::MediaHistoryKeyedService::Get(profile_)->SetKaleidoscopeData(
        response.Clone(), gaia_id);
  }
}

scoped_refptr<::network::SharedURLLoaderFactory>
KaleidoscopeService::GetURLLoaderFactoryForFetcher() {
  if (test_url_loader_factory_for_fetcher_)
    return test_url_loader_factory_for_fetcher_;

  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace kaleidoscope

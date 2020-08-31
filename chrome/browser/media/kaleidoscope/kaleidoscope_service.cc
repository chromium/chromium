// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_service.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_service_factory.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
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
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback)
      : gaia_id_(gaia_id) {
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
        url_loader_factory.get(), std::move(callback));
  }

  ~GetCollectionsRequest() = default;

  std::string gaia_id() const { return gaia_id_; }

 private:
  std::string const gaia_id_;

  std::unique_ptr<::network::SimpleURLLoader> pending_request_;
};

}  // namespace

KaleidoscopeService::KaleidoscopeService(Profile* profile) : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
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

  // If this is a test then return early.
  if (collections_for_testing_.has_value()) {
    std::move(callback).Run(*collections_for_testing_);
    return;
  }

  // Add the callback.
  pending_callbacks_.push_back(std::move(callback));

  // Create the request.
  if (!request_) {
    request_ = std::make_unique<GetCollectionsRequest>(
        std::move(credentials), gaia_id, request,
        GetURLLoaderFactoryForFetcher(),
        base::BindOnce(&KaleidoscopeService::OnURLFetchComplete,
                       base::Unretained(this)));
  }
}

void KaleidoscopeService::SetCollectionsForTesting(
    const std::string& collections) {
  collections_for_testing_ = collections;
}

void KaleidoscopeService::OnURLFetchComplete(
    std::unique_ptr<std::string> data) {
  request_.reset();

  for (auto& callback : pending_callbacks_) {
    if (!data) {
      std::move(callback).Run("");
    } else {
      std::move(callback).Run(*data);
    }
  }

  pending_callbacks_.clear();
}

scoped_refptr<::network::SharedURLLoaderFactory>
KaleidoscopeService::GetURLLoaderFactoryForFetcher() {
  if (test_url_loader_factory_for_fetcher_)
    return test_url_loader_factory_for_fetcher_;

  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace kaleidoscope

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"

#include <memory>
#include <utility>

#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"

namespace {
// The scope required for an access token in order to query Memories.
constexpr char kPhotosScope[] =
    "https://www.googleapis.com/auth/photos.firstparty.readonly";
constexpr char kPhotosImgScope[] =
    "https://www.googleapis.com/auth/photos.image.readonly";
// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;
const char server_url[] =
    "https://photosfirstparty-pa.googleapis.com/chrome_ntp/"
    "read_reminiscing_content";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("photos_service", R"(
      semantics {
        sender: "Photos Service"
        description:
          "The Photos Service requests Memories from "
          "the Google Photos API. The response will be displayed in NTP's "
          "Photos Module."
        trigger:
          "Each time a user navigates to the NTP while "
          "the Photos module is enabled and the user is "
          "signed in."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can control this feature by (1) selecting "
          "a non-Google default search engine in Chrome "
          "settings under 'Search Engine', (2) signing out, "
          "or (3) disabling the Drive module."
        chrome_policy {
          DefaultSearchProviderEnabled {
            policy_options {mode: MANDATORY}
            DefaultSearchProviderEnabled: false
          }
          BrowserSignin {
            policy_options {mode: MANDATORY}
            BrowserSignin: 0
          }
          NTPCardsVisible {
            NTPCardsVisible: false
          }
        }
      })");
}  // namespace

PhotosService::~PhotosService() = default;

PhotosService::PhotosService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {}

void PhotosService::GetMemories(GetMemoriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signin::ScopeSet scopes;
  scopes.insert(kPhotosScope);
  scopes.insert(kPhotosImgScope);
  // TODO(crbug.com/1230867): Handle multiple in-flight requests
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ntp_photos_module", identity_manager_, scopes,
      base::BindOnce(&PhotosService::OnTokenReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void PhotosService::OnTokenReceived(GetMemoriesCallback callback,
                                    GoogleServiceAuthError error,
                                    signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(server_url);
  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ignore cache for fresh results.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);

  if (url_loader_) {
    return;
  }
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PhotosService::OnJsonReceived, weak_factory_.GetWeakPtr(),
                     std::move(callback), token_info.token),
      kMaxResponseSize);
}

void PhotosService::OnJsonReceived(
    GetMemoriesCallback callback,
    const std::string& token,
    const std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  url_loader_.reset();

  if (net_error != net::OK || !response_body) {
    std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&PhotosService::OnJsonParsed, weak_factory_.GetWeakPtr(),
                     std::move(callback), token));
}

void PhotosService::OnJsonParsed(
    GetMemoriesCallback callback,
    const std::string& token,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    return;
  }

  auto* memories = result.value->FindListPath("bundle");
  if (!memories) {
    std::move(callback).Run(std::vector<photos::mojom::MemoryPtr>());
    return;
  }
  std::vector<photos::mojom::MemoryPtr> memory_list;
  for (const auto& memory : memories->GetList()) {
    auto* title = memory.FindStringPath("title.header");
    auto* memory_id = memory.FindStringPath("bundleKey");
    if (!title || !memory_id) {
      continue;
    }
    auto mojo_memory = photos::mojom::Memory::New();
    mojo_memory->title = *title;
    mojo_memory->id = *memory_id;

    memory_list.push_back(std::move(mojo_memory));
  }
  std::move(callback).Run(std::move(memory_list));
}

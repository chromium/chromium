// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service.h"

#include <memory>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "net/base/load_flags.h"

namespace {
// The scope required for an access token in order to query ItemSuggest.
constexpr char kDriveScope[] = "https://www.googleapis.com/auth/drive.readonly";
#if OS_LINUX
constexpr char kPlatform[] = "LINUX";
#elif OS_WIN
constexpr char kPlatform[] = "WINDOWS";
#elif OS_MAC
constexpr char kPlatform[] = "MAC_OS";
#elif OS_CHROMEOS
constexpr char kPlatform[] = "CHROME_OS";
#else
constexpr char kPlatform[] = "UNSPECIFIED_PLATFORM";
#endif
// TODO(crbug/1178869): Add language code to request.
constexpr char kRequestBody[] = R"({
  'client_info': {
    'platform_type': '%s',
    'scenario_type': 'CHROME_NTP_FILES',
    'request_type': 'LIVE_REQUEST'
  },
  'max_suggestions': 3,
  'type_detail_fields': 'drive_item.title,drive_item.mimeType'
})";
// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;
const char server_url[] = "https://appsitemsuggest-pa.googleapis.com/v1/items";
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("drive_service", R"(
      semantics {
        sender: "Drive Service"
        description:
          "The Drive Service requests suggestions for Drive files from "
          "the Drive ItemSuggest API. The response will be displayed in NTP's "
          "Drive Module."
        trigger:
          "Each time a user navigates to the NTP while "
          "the Drive module is enabled and the user is "
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

DriveService::~DriveService() = default;

DriveService::DriveService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {}

void DriveService::GetDriveFiles(GetFilesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.push_back(std::move(callback));
  if (callbacks_.size() > 1) {
    return;
  }

  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ntp_drive_module", identity_manager_, signin::ScopeSet({kDriveScope}),
      base::BindOnce(&DriveService::OnTokenReceived,
                     weak_factory_.GetWeakPtr()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void DriveService::OnTokenReceived(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<drive::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
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

  DCHECK(!url_loader_);
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->AttachStringForUpload(
      base::StringPrintf(kRequestBody, kPlatform), "application/json");
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&DriveService::OnJsonReceived, weak_factory_.GetWeakPtr()),
      kMaxResponseSize);
}

void DriveService::OnJsonReceived(
    const std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  url_loader_.reset();

  if (net_error != net::OK || !response_body) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<drive::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&DriveService::OnJsonParsed, weak_factory_.GetWeakPtr()));
}

void DriveService::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<drive::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }
  auto* items = result.value->FindListPath("item");
  if (!items) {
    for (auto& callback : callbacks_) {
      std::move(callback).Run(std::vector<drive::mojom::FilePtr>());
    }
    callbacks_.clear();
    return;
  }
  for (auto& callback : callbacks_) {
    std::vector<drive::mojom::FilePtr> document_list;
    for (const auto& item : items->GetList()) {
      auto* title = item.FindStringPath("driveItem.title");
      auto* mime_type = item.FindStringPath("driveItem.mimeType");
      auto* justification_text_segments =
          item.FindListPath("justification.displayText.textSegment");
      if (!justification_text_segments ||
          justification_text_segments->GetList().size() == 0) {
        continue;
      }
      std::string justification_text;
      for (auto& text_segment : justification_text_segments->GetList()) {
        auto* justification_text_path = text_segment.FindStringPath("text");
        if (!justification_text_path) {
          continue;
        }
        justification_text += *justification_text_path;
      }
      auto* id = item.FindStringKey("itemId");
      auto* item_url = item.FindStringKey("url");
      if (!title || !mime_type || justification_text.empty() || !id ||
          !item_url || !GURL(*item_url).is_valid()) {
        continue;
      }
      auto* photo_url =
          item.FindStringPath("justification.primaryPerson.photoUrl");
      auto mojo_drive_doc = drive::mojom::File::New();
      mojo_drive_doc->title = *title;
      mojo_drive_doc->mime_type = *mime_type;
      mojo_drive_doc->justification_text = justification_text;
      mojo_drive_doc->id = *id;
      mojo_drive_doc->item_url = GURL(*item_url);
      if (photo_url && GURL(*photo_url).is_valid()) {
        mojo_drive_doc->untrusted_photo_url = GURL(*photo_url);
      }
      document_list.push_back(std::move(mojo_drive_doc));
    }
    std::move(callback).Run(std::move(document_list));
  }
  callbacks_.clear();
}

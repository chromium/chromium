// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/storage_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/devtools/protocol/storage.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

PrivateVerificationTokensService* GetPrivateVerificationTokensService(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return PrivateVerificationTokensServiceFactory::GetForProfile(profile);
}

}  // namespace

StorageHandler::StorageHandler(content::WebContents* web_contents,
                               protocol::UberDispatcher* dispatcher)
    : web_contents_(web_contents->GetWeakPtr()) {
  frontend_ =
      std::make_unique<protocol::Storage::Frontend>(dispatcher->channel());
  protocol::Storage::Dispatcher::wire(dispatcher, this);
}

StorageHandler::~StorageHandler() = default;

protocol::Response StorageHandler::Disable() {
  pvt_observation_.Reset();
  return protocol::Response::FallThrough();
}

// TODO: crbug.com/380896828 - move CDP support for BTM to //content.
void StorageHandler::RunBounceTrackingMitigations(
    std::unique_ptr<RunBounceTrackingMitigationsCallback> callback) {
  content::BtmService* btm_service =
      web_contents_
          ? content::BtmService::Get(web_contents_->GetBrowserContext())
          : nullptr;

  if (!btm_service) {
    callback->sendFailure(protocol::Response::ServerError("No BtmService"));
    return;
  }

  btm_service->DeleteEligibleSitesImmediately(
      base::BindOnce(&StorageHandler::GotDeletedSites, std::move(callback)));
}

/* static */
void StorageHandler::GotDeletedSites(
    std::unique_ptr<RunBounceTrackingMitigationsCallback> callback,
    const std::vector<std::string>& sites) {
  auto deleted_sites =
      std::make_unique<protocol::Array<protocol::String>>(sites);
  callback->sendSuccess(std::move(deleted_sites));
}

void StorageHandler::GetPrivateVerificationTokens(
    std::unique_ptr<GetPrivateVerificationTokensCallback> callback) {
  PrivateVerificationTokensService* pvt_service =
      GetPrivateVerificationTokensService(web_contents_.get());
  if (!pvt_service) {
    callback->sendFailure(protocol::Response::ServerError(
        "Private Verification Tokens service is not available"));
    return;
  }

  pvt_service->GetAllTokens(base::BindOnce(
      [](std::unique_ptr<GetPrivateVerificationTokensCallback> callback,
         std::vector<private_verification_tokens::TokenWithId> tokens) {
        auto result = std::make_unique<
            protocol::Array<protocol::Storage::PrivateVerificationToken>>();
        for (const auto& item : tokens) {
          result->push_back(
              protocol::Storage::PrivateVerificationToken::Create()
                  .SetId(base::NumberToString(item.id))
                  .SetIssuerOrigin(item.token.issuer().Serialize())
                  .SetKeyId(static_cast<int>(item.token.key_id()))
                  .SetExpiration(
                      item.token.expiration().InSecondsFSinceUnixEpoch())
                  .SetCreationTime(
                      item.token.creation_time().InSecondsFSinceUnixEpoch())
                  .SetVersion(static_cast<int>(item.token.version()))
                  .SetToken(base::Base64Encode(item.token.token()))
                  .Build());
        }
        callback->sendSuccess(std::move(result));
      },
      std::move(callback)));
}

void StorageHandler::ClearPrivateVerificationTokens(
    const std::string& in_issuerOrigin,
    std::unique_ptr<ClearPrivateVerificationTokensCallback> callback) {
  PrivateVerificationTokensService* pvt_service =
      GetPrivateVerificationTokensService(web_contents_.get());
  if (!pvt_service) {
    callback->sendFailure(protocol::Response::ServerError(
        "Private Verification Tokens service is not available"));
    return;
  }

  url::Origin issuer_origin = url::Origin::Create(GURL(in_issuerOrigin));
  if (issuer_origin.opaque()) {
    callback->sendFailure(
        protocol::Response::InvalidParams("Invalid issuer origin"));
    return;
  }

  pvt_service->DeleteTokens(
      base::Time::Min(), base::Time::Max(),
      base::BindOnce(&ClearPrivateVerificationTokensCallback::sendSuccess,
                     std::move(callback)),
      std::vector<url::Origin>{issuer_origin});
}

void StorageHandler::DeletePrivateVerificationToken(
    const std::string& in_tokenId,
    std::unique_ptr<DeletePrivateVerificationTokenCallback> callback) {
  PrivateVerificationTokensService* pvt_service =
      GetPrivateVerificationTokensService(web_contents_.get());
  if (!pvt_service) {
    callback->sendFailure(protocol::Response::ServerError(
        "Private Verification Tokens service is not available"));
    return;
  }

  int64_t token_id = 0;
  if (!base::StringToInt64(in_tokenId, &token_id)) {
    callback->sendFailure(
        protocol::Response::InvalidParams("Invalid token ID"));
    return;
  }

  pvt_service->DeleteToken(
      token_id,
      base::BindOnce(&DeletePrivateVerificationTokenCallback::sendSuccess,
                     std::move(callback)));
}

protocol::Response StorageHandler::SetPrivateVerificationTokensTracking(
    bool enable) {
  PrivateVerificationTokensService* pvt_service =
      GetPrivateVerificationTokensService(web_contents_.get());
  if (!pvt_service) {
    return protocol::Response::ServerError(
        "Private Verification Tokens service is not available");
  }

  if (enable) {
    if (!pvt_observation_.IsObserving()) {
      pvt_observation_.Observe(pvt_service);
    }
  } else {
    pvt_observation_.Reset();
  }
  return protocol::Response::Success();
}

void StorageHandler::OnTokensStored() {
  frontend_->PrivateVerificationTokensUpdated();
}

void StorageHandler::OnTokensDeleted() {
  frontend_->PrivateVerificationTokensUpdated();
}

void StorageHandler::OnShutdown() {
  pvt_observation_.Reset();
}

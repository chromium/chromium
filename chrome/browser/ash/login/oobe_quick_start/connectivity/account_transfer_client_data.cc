// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/account_transfer_client_data.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "url/gurl.h"

namespace ash::quick_start {

AccountTransferClientData::AccountTransferClientData(
    Base64UrlString challenge_b64url)
    : challenge_b64url_(challenge_b64url) {}

AccountTransferClientData::~AccountTransferClientData() = default;

Base64UrlString AccountTransferClientData::GetChallengeBase64URLString() {
  return challenge_b64url_;
}

std::string AccountTransferClientData::CreateJson() {
  base::Value::Dict fido_collected_client_data;
  url::Origin origin = url::Origin::Create(GURL(kOrigin));
  fido_collected_client_data.Set(kClientDataOriginKey, origin.Serialize());
  fido_collected_client_data.Set(kClientDataTypeKey, kCtapRequestType);
  fido_collected_client_data.Set(
      kClientDataChallengeKey,
      std::string(challenge_b64url_->begin(), challenge_b64url_->end()));
  fido_collected_client_data.Set(kClientDataCrossOriginKey, false);
  return base::WriteJson(fido_collected_client_data).value_or("");
}

std::array<uint8_t, crypto::hash::kSha256Size>
AccountTransferClientData::CreateHash() {
  return crypto::hash::Sha256(base::as_byte_span(CreateJson()));
}

}  // namespace ash::quick_start

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

AccountTransferClientData::~AccountTransferClientData() {}

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
  std::string fido_client_data_json;
  base::JSONWriter::Write(fido_collected_client_data, &fido_client_data_json);
  return fido_client_data_json;
}

std::array<uint8_t, crypto::kSHA256Length>
AccountTransferClientData::CreateHash() {
  std::string json = CreateJson();
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(json, client_data_hash.data(),
                           client_data_hash.size());

  return client_data_hash;
}

}  // namespace ash::quick_start

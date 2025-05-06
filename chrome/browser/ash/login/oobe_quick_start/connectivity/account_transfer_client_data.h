// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_ACCOUNT_TRANSFER_CLIENT_DATA_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_ACCOUNT_TRANSFER_CLIENT_DATA_H_

#include <string>

#include "chromeos/ash/components/quick_start/types.h"
#include "crypto/hash.h"
#include "url/origin.h"

namespace ash::quick_start {

inline constexpr char kClientDataTypeKey[] = "type";
inline constexpr char kClientDataChallengeKey[] = "challenge";
inline constexpr char kClientDataOriginKey[] = "origin";
inline constexpr char kClientDataCrossOriginKey[] = "crossOrigin";
inline constexpr char kCtapRequestType[] = "webauthn.get";

inline constexpr char kOrigin[] = "https://accounts.google.com";

// AccountTransferClientData represents the client_data payload sent during the
// FIDO Assertion flow, and handles encoding/decoding this payload into various
// formats consumed in the flow (JSON, hash string, etc).
class AccountTransferClientData {
 public:
  explicit AccountTransferClientData(Base64UrlString challenge_b64url);
  ~AccountTransferClientData();

  std::string CreateJson();
  std::array<uint8_t, crypto::hash::kSha256Size> CreateHash();

  Base64UrlString GetChallengeBase64URLString();

 private:
  Base64UrlString challenge_b64url_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_ACCOUNT_TRANSFER_CLIENT_DATA_H_

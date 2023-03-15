// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/incoming_connection.h"

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "crypto/random.h"

namespace ash::quick_start {

IncomingConnection::IncomingConnection(NearbyConnection* nearby_connection,
                                       RandomSessionId session_id,
                                       const std::string& authentication_token)
    : Connection(nearby_connection, session_id),
      pin_(DerivePin(authentication_token)) {}

IncomingConnection::IncomingConnection(NearbyConnection* nearby_connection,
                                       RandomSessionId session_id,
                                       const std::string& authentication_token,
                                       SharedSecret shared_secret)
    : Connection(nearby_connection, session_id, shared_secret),
      pin_(DerivePin(authentication_token)) {}

IncomingConnection::~IncomingConnection() = default;

std::string IncomingConnection::DerivePin(
    const std::string& authentication_token) {
  std::string hash_str = base::SHA1HashString(authentication_token);
  std::vector<int8_t> hash_ints =
      std::vector<int8_t>(hash_str.begin(), hash_str.end());

  return base::NumberToString(
             std::abs((hash_ints[0] << 8 | hash_ints[1]) % 10)) +
         base::NumberToString(
             std::abs((hash_ints[2] << 8 | hash_ints[3]) % 10)) +
         base::NumberToString(
             std::abs((hash_ints[4] << 8 | hash_ints[5]) % 10)) +
         base::NumberToString(
             std::abs((hash_ints[6] << 8 | hash_ints[7]) % 10));
}

std::vector<uint8_t> IncomingConnection::GetQrCodeData() const {
  std::string shared_secret_str(shared_secret_.begin(), shared_secret_.end());
  std::string shared_secret_base64;
  base::Base64UrlEncode(shared_secret_str,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &shared_secret_base64);

  std::string url = "https://signin.google/qs/" +
                    random_session_id_.ToString() +
                    "?key=" + shared_secret_base64;

  return std::vector<uint8_t>(url.begin(), url.end());
}

}  // namespace ash::quick_start

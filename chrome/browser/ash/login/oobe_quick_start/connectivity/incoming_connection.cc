// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/incoming_connection.h"

#include "base/base64url.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "crypto/random.h"

namespace ash::quick_start {

IncomingConnection::IncomingConnection(NearbyConnection* nearby_connection,
                                       RandomSessionId session_id)
    : Connection(nearby_connection), random_session_id_(session_id) {
  crypto::RandBytes(shared_secret_);
}

IncomingConnection::IncomingConnection(NearbyConnection* nearby_connection,
                                       RandomSessionId session_id,
                                       std::array<uint8_t, 32> shared_secret)
    : Connection(nearby_connection),
      random_session_id_(session_id),
      shared_secret_(shared_secret) {}

IncomingConnection::~IncomingConnection() = default;

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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "crypto/random.h"

namespace ash::quick_start {

Connection::Connection(NearbyConnection* nearby_connection,
                       RandomSessionId session_id,
                       SharedSecret shared_secret)
    : nearby_connection_(nearby_connection),
      random_session_id_(session_id),
      shared_secret_(shared_secret) {}

Connection::Connection(NearbyConnection* nearby_connection,
                       RandomSessionId session_id)
    : nearby_connection_(nearby_connection), random_session_id_(session_id) {
  crypto::RandBytes(shared_secret_);
}

void Connection::SendPayload(const base::Value::Dict& message_payload) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(message_payload, &json_serialized_payload));
  std::vector<uint8_t> request_payload(json_serialized_payload.begin(),
                                       json_serialized_payload.end());
  nearby_connection_->Write(request_payload);
}

void Connection::SendPayloadAndReadResponse(
    const base::Value::Dict& message_payload,
    PayloadResponseCallback callback) {
  SendPayload(message_payload);
  nearby_connection_->Read(std::move(callback));
}

}  // namespace ash::quick_start

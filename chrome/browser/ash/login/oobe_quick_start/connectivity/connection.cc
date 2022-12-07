// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

Connection::Connection(NearbyConnection* nearby_connection)
    : nearby_connection_(nearby_connection) {}

void Connection::SendPayload(const base::Value::Dict& message_payload) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(message_payload, &json_serialized_payload));
  std::vector<uint8_t> request_payload(json_serialized_payload.begin(),
                                       json_serialized_payload.end());
  nearby_connection_->Write(request_payload);
}

}  // namespace ash::quick_start

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_client_base.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/quick_start_decoder.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

TargetDeviceClientBase::TargetDeviceClientBase(
    NearbyConnection* nearby_connection,
    QuickStartDecoder* quick_start_decoder)
    : nearby_connection_(nearby_connection),
      quick_start_decoder_(quick_start_decoder) {
  CHECK(nearby_connection_);
  // TODO(b/258680767): Uncomment this after we can pass in QuickStartDecoder.
  // CHECK(quick_start_decoder_);
}

TargetDeviceClientBase::~TargetDeviceClientBase() = default;

void TargetDeviceClientBase::SendPayload(
    const base::Value::Dict& message_payload) {
  std::string json_serialized_payload;
  CHECK(base::JSONWriter::Write(message_payload, &json_serialized_payload));
  std::vector<uint8_t> request_payload(json_serialized_payload.begin(),
                                       json_serialized_payload.end());
  nearby_connection_->Write(request_payload);
  nearby_connection_->Read(base::BindOnce(&TargetDeviceClientBase::OnDataRead,
                                          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash::quick_start

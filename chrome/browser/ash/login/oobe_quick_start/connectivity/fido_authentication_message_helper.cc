// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fido_authentication_message_helper.h"

#include "components/cbor/values.h"
#include "components/cbor/writer.h"

namespace ash::quick_start::message_helper {

std::vector<uint8_t> BuildEncodedResponseData(
    std::vector<uint8_t> credential_id,
    std::vector<uint8_t> auth_data,
    std::vector<uint8_t> signature,
    std::vector<uint8_t> user_id,
    uint8_t status) {
  cbor::Value::MapValue cbor_map;
  cbor::Value::MapValue credential_map;
  credential_map[cbor::Value(kCredentialIdKey)] = cbor::Value(credential_id);

  cbor_map[cbor::Value(kCborTypeInt)] = cbor::Value(credential_map);
  cbor_map[cbor::Value(kCborTypeByteString)] = cbor::Value(auth_data);
  cbor_map[cbor::Value(kCborTypeString)] = cbor::Value(signature);
  cbor::Value::MapValue user_map;
  user_map[cbor::Value(kEntitiyIdMapKey)] = cbor::Value(user_id);
  cbor_map[cbor::Value(kCborTypeArray)] = cbor::Value(user_map);
  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(cbor_bytes);
  std::vector<uint8_t> response_bytes = std::move(*cbor_bytes);
  // Add the status byte to the beginning of this now fully encoded cbor bytes
  // vector.
  response_bytes.insert(response_bytes.begin(), status);
  return response_bytes;
}
}  // namespace ash::quick_start::message_helper

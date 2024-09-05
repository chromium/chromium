// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_CHAPS_MOCK_SESSION_CHAPS_CLIENT_H_
#define ASH_COMPONENTS_KCER_CHAPS_MOCK_SESSION_CHAPS_CLIENT_H_

#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace kcer {

class MockSessionChapsClient : public SessionChapsClient {
 public:
  MockSessionChapsClient();
  ~MockSessionChapsClient() override;

  MOCK_METHOD(void,
              GetMechanismList,
              (SlotId slot_id, GetMechanismListCallback callback),
              (override));
  MOCK_METHOD(void,
              CreateObject,
              (SlotId slot_id,
               const std::vector<uint8_t>& attributes,
               int attempts_left,
               CreateObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAttributeValue,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::ObjectHandle object_handle,
               std::vector<uint8_t> attributes_query,
               int attempts_left,
               SessionChapsClient::GetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAttributeValue,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::ObjectHandle object_handle,
               std::vector<uint8_t> attributes,
               int attempts_left,
               SessionChapsClient::SetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              DestroyObject,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::ObjectHandle object_handle,
               int attempts_left,
               SessionChapsClient::DestroyObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjects,
              (SessionChapsClient::SlotId slot_id,
               std::vector<uint8_t> attributes,
               int attempts_left,
               SessionChapsClient::FindObjectsCallback callback),
              (override));
  MOCK_METHOD(void,
              Sign,
              (SessionChapsClient::SlotId slot_id,
               uint64_t mechanism_type,
               std::vector<uint8_t> mechanism_parameter,
               SessionChapsClient::ObjectHandle key_handle,
               std::vector<uint8_t> data,
               int attempts_left,
               SessionChapsClient::SignCallback callback),
              (override));
  MOCK_METHOD(void,
              GenerateKeyPair,
              (SessionChapsClient::SlotId slot_id,
               uint64_t mechanism_type,
               std::vector<uint8_t> mechanism_parameter,
               std::vector<uint8_t> public_key_attributes,
               std::vector<uint8_t> private_key_attributes,
               int attempts_left,
               SessionChapsClient::GenerateKeyPairCallback callback),
              (override));
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_CHAPS_MOCK_SESSION_CHAPS_CLIENT_H_

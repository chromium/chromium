// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_CHAPS_MOCK_HIGH_LEVEL_CHAPS_CLIENT_H_
#define ASH_COMPONENTS_KCER_CHAPS_MOCK_HIGH_LEVEL_CHAPS_CLIENT_H_

#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace kcer {

class COMPONENT_EXPORT(KCER) MockHighLevelChapsClient
    : public HighLevelChapsClient {
 public:
  MockHighLevelChapsClient();
  ~MockHighLevelChapsClient() override;

  MOCK_METHOD(void,
              GetMechanismList,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::GetMechanismListCallback callback),
              (override));
  MOCK_METHOD(void,
              CreateObject,
              (SessionChapsClient::SlotId slot_id,
               const chaps::AttributeList& attributes,
               SessionChapsClient::CreateObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              DestroyObject,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::ObjectHandle object_handle,
               SessionChapsClient::DestroyObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              DestroyObjectsWithRetries,
              (SessionChapsClient::SlotId slot_id,
               std::vector<SessionChapsClient::ObjectHandle> object_handles,
               SessionChapsClient::DestroyObjectCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAttributeValue,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::ObjectHandle object_handle,
               std::vector<AttributeId> attribute_ids,
               HighLevelChapsClient::GetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAttributeValue,
              (SessionChapsClient::SlotId slot_id,
               SessionChapsClient::ObjectHandle object_handle,
               const chaps::AttributeList& attributes,
               SessionChapsClient::SetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAttributeValue,
              (SessionChapsClient::SlotId slot_id,
               std::vector<SessionChapsClient::ObjectHandle> object_handles,
               const chaps::AttributeList& attributes,
               SessionChapsClient::SetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjects,
              (SessionChapsClient::SlotId slot_id,
               const chaps::AttributeList& attributes,
               SessionChapsClient::FindObjectsCallback callback),
              (override));
  MOCK_METHOD(void,
              Sign,
              (SessionChapsClient::SlotId slot_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               SessionChapsClient::ObjectHandle key_handle,
               std::vector<uint8_t> data,
               SessionChapsClient::SignCallback callback),
              (override));
  MOCK_METHOD(void,
              GenerateKeyPair,
              (SessionChapsClient::SlotId slot_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               const chaps::AttributeList& public_key_attributes,
               const chaps::AttributeList& private_key_attributes,
               SessionChapsClient::GenerateKeyPairCallback callback),
              (override));
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_CHAPS_MOCK_HIGH_LEVEL_CHAPS_CLIENT_H_

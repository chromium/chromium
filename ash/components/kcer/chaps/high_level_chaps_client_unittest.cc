// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/chaps/high_level_chaps_client.h"

#include "ash/components/kcer/attributes.pb.h"
#include "ash/components/kcer/chaps/mock_session_chaps_client.h"
#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "base/containers/contains.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using SlotId = kcer::SessionChapsClient::SlotId;
using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using testing::_;
using testing::DoAll;
using testing::UnorderedElementsAre;

namespace kcer {
namespace {

class HighLevelChapsClientTest : public testing::Test {
 public:
  int32_t GetFakeAttributeSize(uint32_t type) {
    // The main goal is to provide mostly unique values, so it's easy to verify
    // that they are forwarded to correct places. A buffer will actually be
    // allocated for the returned size, so limit it to 127 bytes.
    return type % 128;
  }

  chaps::AttributeList GetFakeAttributes(size_t seed) {
    // Since it affects the size, limit it to 19.
    seed = seed % 20;

    // Create 3 attributes which types and values depend on the `seed`.
    chaps::AttributeList attributes;
    for (size_t i = seed; i < seed + 3; ++i) {
      chaps::Attribute* attr = attributes.add_attributes();
      attr->set_type(i);
      attr->set_length(i);
      std::string value(/*count=*/5 + i, /*ch=*/'a');
      attr->set_value(value.data(), value.length());
    }
    return attributes;
  }

 protected:
  MockSessionChapsClient mock_session_client_;
  HighLevelChapsClientImpl client_{&mock_session_client_};
};

// Test that CreateObject correctly translates its arguments and invokes
// SessionChapsClient.
TEST_F(HighLevelChapsClientTest, CreateObject) {
  constexpr SlotId kSlotId(1);
  const chaps::AttributeList kAttributes = GetFakeAttributes(0);

  std::vector<uint8_t> attributes_bytes;
  SessionChapsClient::CreateObjectCallback callback;
  EXPECT_CALL(mock_session_client_, CreateObject(kSlotId, _, _, _))
      .WillOnce(DoAll(MoveArg<1>(&attributes_bytes), MoveArg<3>(&callback)));

  base::test::TestFuture<ObjectHandle, uint32_t> waiter;
  client_.CreateObject(kSlotId, kAttributes, waiter.GetCallback());

  // Emulate that `mock_session_client_` returned something.
  ASSERT_TRUE(callback);
  ObjectHandle result_handle(20);
  uint32_t result_code = 300;
  std::move(callback).Run(result_handle, result_code);

  chaps::AttributeList received_attributes;
  received_attributes.ParseFromArray(attributes_bytes.data(),
                                     attributes_bytes.size());
  EXPECT_EQ(received_attributes.SerializeAsString(),
            kAttributes.SerializeAsString());

  // Check that the result code was forwarded correctly.
  EXPECT_EQ(waiter.Get<ObjectHandle>(), result_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DestroyObject correctly translates its arguments and invokes
// SessionChapsClient.
TEST_F(HighLevelChapsClientTest, DestroyObject) {
  constexpr SlotId kSlotId(1);
  constexpr ObjectHandle kObjectHandle(20);

  SessionChapsClient::DestroyObjectCallback callback;
  EXPECT_CALL(mock_session_client_, DestroyObject(kSlotId, kObjectHandle, _, _))
      .WillOnce(MoveArg<3>(&callback));

  base::test::TestFuture<uint32_t> waiter;
  client_.DestroyObject(kSlotId, kObjectHandle, waiter.GetCallback());

  // Emulate that `mock_session_client_` returned something.
  ASSERT_TRUE(callback);
  uint32_t result_code = 300;
  std::move(callback).Run(result_code);

  // Check that the result code was forwarded correctly.
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DestroyObjectsWithRetries correctly invokes SessionChapsClient when
// it always returns success.
TEST_F(HighLevelChapsClientTest, DestroyObjectsWithRetriesFullSuccess) {
  constexpr SlotId kSlotId(1);
  const std::vector<ObjectHandle> kObjectHandles{
      ObjectHandle(20), ObjectHandle(30), ObjectHandle(40)};

  for (ObjectHandle h : kObjectHandles) {
    EXPECT_CALL(mock_session_client_, DestroyObject(kSlotId, h, _, _))
        .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_OK));
  }

  base::test::TestFuture<uint32_t> waiter;
  client_.DestroyObjectsWithRetries(kSlotId, kObjectHandles,
                                    waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
}

// Test that DestroyObjectsWithRetries correctly invokes SessionChapsClient when
// it returns an error for a single object.
TEST_F(HighLevelChapsClientTest, DestroyObjectsWithRetriesPartialSuccess) {
  constexpr SlotId kSlotId(1);
  const std::vector<ObjectHandle> kObjectHandles{
      ObjectHandle(20), ObjectHandle(30), ObjectHandle(40)};

  EXPECT_CALL(mock_session_client_,
              DestroyObject(kSlotId, ObjectHandle(20), _, _))
      .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_OK));
  // Make the call repeatedly fail on one of the objects.
  EXPECT_CALL(mock_session_client_,
              DestroyObject(kSlotId, ObjectHandle(30), _, _))
      .Times(3)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<3>(chromeos::PKCS11_CKR_GENERAL_ERROR));
  EXPECT_CALL(mock_session_client_,
              DestroyObject(kSlotId, ObjectHandle(40), _, _))
      .WillOnce(RunOnceCallback<3>(chromeos::PKCS11_CKR_OK));

  base::test::TestFuture<uint32_t> waiter;
  client_.DestroyObjectsWithRetries(kSlotId, kObjectHandles,
                                    waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_GENERAL_ERROR);
}

// Test that DestroyObjectsWithRetries correctly invokes SessionChapsClient when
// it returns an error for all objects.
TEST_F(HighLevelChapsClientTest, DestroyObjectsWithRetriesFullFailure) {
  constexpr SlotId kSlotId(1);
  const std::vector<ObjectHandle> kObjectHandles{
      ObjectHandle(20), ObjectHandle(30), ObjectHandle(40)};

  for (ObjectHandle h : kObjectHandles) {
    EXPECT_CALL(mock_session_client_, DestroyObject(kSlotId, h, _, _))
        .Times(3)
        .WillRepeatedly(
            RunOnceCallbackRepeatedly<3>(chromeos::PKCS11_CKR_GENERAL_ERROR));
  }

  base::test::TestFuture<uint32_t> waiter;
  client_.DestroyObjectsWithRetries(kSlotId, kObjectHandles,
                                    waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_GENERAL_ERROR);
}

// Test that GetAttributeValue correctly calls SessionChapsClient when the
// attributes are successfully returned after the initial request.
TEST_F(HighLevelChapsClientTest, GetAttributeValueFirstTrySuccess) {
  constexpr SlotId kSlotId(1);
  constexpr ObjectHandle kObjectHandle(20);
  std::vector<HighLevelChapsClient::AttributeId> kAttributeIds{
      HighLevelChapsClient::AttributeId::kModulus,
      HighLevelChapsClient::AttributeId::kPublicExponent};

  std::vector<uint8_t> attributes_query;
  SessionChapsClient::GetAttributeValueCallback callback;
  EXPECT_CALL(mock_session_client_,
              GetAttributeValue(kSlotId, kObjectHandle, _, _, _))
      .WillOnce(DoAll(MoveArg<2>(&attributes_query), MoveArg<4>(&callback)));

  base::test::TestFuture<chaps::AttributeList, uint32_t> waiter;
  client_.GetAttributeValue(kSlotId, kObjectHandle, kAttributeIds,
                            waiter.GetCallback());

  chaps::AttributeList requested_attrs;
  requested_attrs.ParseFromArray(attributes_query.data(),
                                 attributes_query.size());
  std::vector<HighLevelChapsClient::AttributeId> requested_attr_ids;
  // Check what attributes were requested and populate some values to return
  // them.
  for (int i = 0; i < requested_attrs.attributes_size(); i++) {
    chaps::Attribute* attr = requested_attrs.mutable_attributes(i);
    requested_attr_ids.push_back(
        static_cast<HighLevelChapsClient::AttributeId>(attr->type()));
    // The caller is supposed to set the "length" correctly and put a buffer
    // into the "value". The fact the this code overrides them is not very
    // realistic, but good enough for the success scenario.
    EXPECT_GT(attr->length(), 0);
    EXPECT_TRUE(attr->has_value());
    std::string value(/*count=*/5 + i, /*ch=*/'a');
    attr->set_value(value.data(), value.size());
    attr->set_length(value.size());
  }
  EXPECT_THAT(requested_attr_ids,
              testing::UnorderedElementsAreArray(kAttributeIds));
  // Emulate a reply from SessionChapsClient.
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  std::move(callback).Run(SessionChapsClient::SerializeToBytes(requested_attrs),
                          result_code);

  // Check that the reply was forwarded correctly.
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
  EXPECT_EQ(waiter.Get<chaps::AttributeList>().SerializeAsString(),
            requested_attrs.SerializeAsString());
}

// Test that GetAttributeValue correctly invokes SessionChapsClient when it
// replies that the allocated buffer is too small.
TEST_F(HighLevelChapsClientTest, GetAttributeValueGetSizeSuccess) {
  constexpr SlotId kSlotId(1);
  const ObjectHandle kObjectHandle(20);
  const std::vector<HighLevelChapsClient::AttributeId> kAttributeIds{
      HighLevelChapsClient::AttributeId::kModulus,
      HighLevelChapsClient::AttributeId::kPublicExponent};

  std::vector<uint8_t> attributes_query;
  SessionChapsClient::GetAttributeValueCallback callback;
  EXPECT_CALL(mock_session_client_,
              GetAttributeValue(kSlotId, kObjectHandle, _, _, _))
      .Times(3)
      .WillRepeatedly(
          DoAll(MoveArg<2>(&attributes_query), MoveArg<4>(&callback)));

  base::test::TestFuture<chaps::AttributeList, uint32_t> waiter;
  client_.GetAttributeValue(kSlotId, kObjectHandle, kAttributeIds,
                            waiter.GetCallback());

  // `attributes_query` and `callback` should get populated now.

  chaps::AttributeList requested_attrs;
  requested_attrs.ParseFromArray(attributes_query.data(),
                                 attributes_query.size());
  for (int i = 0; i < requested_attrs.attributes_size(); i++) {
    chaps::Attribute* attr = requested_attrs.mutable_attributes(i);
    // PKCS#11 standard instructs replacing the length with the error code when
    // the buffer for the attribute is too small.
    attr->set_length(chromeos::PKCS11_CK_UNAVAILABLE_INFORMATION);
  }
  // Emulate a reply from SessionChapsClient.
  std::move(callback).Run(SessionChapsClient::SerializeToBytes(requested_attrs),
                          chromeos::PKCS11_CKR_BUFFER_TOO_SMALL);

  // `client_` is expected to make an additional call to learn the correct
  // buffer size. `attributes_query` and `callback` should get repopulated now.

  // Emulate a reply from SessionChapsClient.
  requested_attrs.ParseFromArray(attributes_query.data(),
                                 attributes_query.size());
  for (int i = 0; i < requested_attrs.attributes_size(); i++) {
    chaps::Attribute* attr = requested_attrs.mutable_attributes(i);
    // PKCS#11 instructs to set the value to a NULL_PTR, the protobuf layer
    // expects to have no value in that case.
    EXPECT_EQ(attr->has_value(), false);
    attr->set_length(GetFakeAttributeSize(attr->type()));
  }
  std::move(callback).Run(SessionChapsClient::SerializeToBytes(requested_attrs),
                          chromeos::PKCS11_CKR_OK);

  // `client_` is expected to retry the original call now, but with the new
  // sizes. `attributes_query` and `callback` should get repopulated now.

  // Populate the values for the new sizes.
  requested_attrs.ParseFromArray(attributes_query.data(),
                                 attributes_query.size());
  for (int i = 0; i < requested_attrs.attributes_size(); i++) {
    chaps::Attribute* attr = requested_attrs.mutable_attributes(i);
    EXPECT_EQ(attr->length(), GetFakeAttributeSize(attr->type()));
    EXPECT_EQ(static_cast<size_t>(attr->length()), attr->value().size());
    std::string value(/*count=*/attr->length(), /*ch=*/'a');
    attr->set_value(value.data(), value.size());
  }
  // Emulate a reply from SessionChapsClient.
  uint32_t result_code = chromeos::PKCS11_CKR_OK;
  std::move(callback).Run(SessionChapsClient::SerializeToBytes(requested_attrs),
                          result_code);

  // Check that the reply was forwarded correctly.
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
  EXPECT_EQ(waiter.Get<chaps::AttributeList>().SerializeAsString(),
            requested_attrs.SerializeAsString());
}

// Test that SetAttributeValue correctly translates its arguments and invokes
// SessionChapsClient.
TEST_F(HighLevelChapsClientTest, SetAttributeValue) {
  constexpr SlotId kSlotId(1);
  constexpr ObjectHandle kObjectHandle(20);
  const chaps::AttributeList kAttributes = GetFakeAttributes(0);
  constexpr uint32_t kResultCode = 300;

  std::vector<uint8_t> attributes_bytes;
  EXPECT_CALL(mock_session_client_,
              SetAttributeValue(kSlotId, kObjectHandle, _, _, _))
      .WillOnce(DoAll(MoveArg<2>(&attributes_bytes),
                      RunOnceCallback<4>(kResultCode)));

  base::test::TestFuture<uint32_t> waiter;
  client_.SetAttributeValue(kSlotId, kObjectHandle, kAttributes,
                            waiter.GetCallback());

  chaps::AttributeList received_attrs;
  received_attrs.ParseFromArray(attributes_bytes.data(),
                                attributes_bytes.size());
  EXPECT_EQ(received_attrs.SerializeAsString(),
            kAttributes.SerializeAsString());
  EXPECT_EQ(waiter.Get<uint32_t>(), kResultCode);
}

// Test that SetAttributeValue correctly translates its arguments and invokes
// SessionChapsClient for each object that needs to be updated.
TEST_F(HighLevelChapsClientTest, SetAttributeValueForMultipleObjects) {
  constexpr SlotId kSlotId(1);
  const std::vector<ObjectHandle> kObjectHandles{ObjectHandle(20),
                                                 ObjectHandle(30)};
  const chaps::AttributeList kAttributes = GetFakeAttributes(1);

  std::vector<uint8_t> attributes_bytes_0;
  EXPECT_CALL(mock_session_client_,
              SetAttributeValue(kSlotId, kObjectHandles[0], _, _, _))
      .WillOnce(DoAll(MoveArg<2>(&attributes_bytes_0),
                      RunOnceCallback<4>(chromeos::PKCS11_CKR_OK)));

  std::vector<uint8_t> attributes_bytes_1;
  EXPECT_CALL(mock_session_client_,
              SetAttributeValue(kSlotId, kObjectHandles[1], _, _, _))
      .WillOnce(DoAll(MoveArg<2>(&attributes_bytes_1),
                      RunOnceCallback<4>(chromeos::PKCS11_CKR_OK)));

  base::test::TestFuture<uint32_t> waiter;
  client_.SetAttributeValue(kSlotId, kObjectHandles, kAttributes,
                            waiter.GetCallback());

  chaps::AttributeList received_attrs_0;
  received_attrs_0.ParseFromArray(attributes_bytes_0.data(),
                                  attributes_bytes_0.size());
  EXPECT_EQ(received_attrs_0.SerializeAsString(),
            kAttributes.SerializeAsString());

  chaps::AttributeList received_attrs_1;
  received_attrs_1.ParseFromArray(attributes_bytes_1.data(),
                                  attributes_bytes_1.size());
  EXPECT_EQ(received_attrs_1.SerializeAsString(),
            kAttributes.SerializeAsString());

  EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
}

// Test that FindObjects correctly translates its arguments and invokes
// SessionChapsClient.
TEST_F(HighLevelChapsClientTest, FindObjects) {
  constexpr SlotId kSlotId(1);
  const chaps::AttributeList kAttributes = GetFakeAttributes(2);
  constexpr uint32_t kResultCode = 300;
  const std::vector<ObjectHandle> kResultHandles{
      ObjectHandle(20), ObjectHandle(30), ObjectHandle(40)};

  std::vector<uint8_t> attributes_bytes;
  EXPECT_CALL(mock_session_client_, FindObjects(kSlotId, _, _, _))
      .WillOnce(DoAll(MoveArg<1>(&attributes_bytes),
                      RunOnceCallback<3>(kResultHandles, kResultCode)));

  base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
  client_.FindObjects(kSlotId, kAttributes, waiter.GetCallback());

  chaps::AttributeList received_attrs;
  received_attrs.ParseFromArray(attributes_bytes.data(),
                                attributes_bytes.size());
  EXPECT_EQ(received_attrs.SerializeAsString(),
            kAttributes.SerializeAsString());

  // Check that the reply was forwarded correctly.
  EXPECT_EQ(waiter.Get<uint32_t>(), kResultCode);
  EXPECT_EQ(waiter.Get<std::vector<ObjectHandle>>(), kResultHandles);
}

// Test that Sign correctly translates its arguments and invokes
// SessionChapsClient.
TEST_F(HighLevelChapsClientTest, Sign) {
  constexpr SlotId kSlotId(1);
  constexpr uint64_t kMechanismType = 22;
  const std::vector<uint8_t> kMechanismParameter{3, 3, 3, 3};
  constexpr ObjectHandle kKeyHandle(33);
  const std::vector<uint8_t> kData{4, 4, 4, 4};
  const std::vector<uint8_t> kResultSignature{5, 5, 5, 5};
  constexpr uint32_t kResultCode = 66;

  EXPECT_CALL(mock_session_client_,
              Sign(kSlotId, kMechanismType, kMechanismParameter, kKeyHandle,
                   kData, _, _))
      .WillOnce(RunOnceCallback<6>(kResultSignature, kResultCode));

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
  client_.Sign(kSlotId, kMechanismType, kMechanismParameter, kKeyHandle, kData,
               waiter.GetCallback());

  // Check that the reply was forwarded correctly.
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), kResultSignature);
  EXPECT_EQ(waiter.Get<uint32_t>(), kResultCode);
}

// Test that GenerateKeyPair correctly translates its arguments and invokes
// SessionChapsClient.
TEST_F(HighLevelChapsClientTest, GenerateKeyPair) {
  constexpr SlotId kSlotId(1);
  constexpr uint64_t kMechanismType = 123;
  const std::vector<uint8_t> kMechanismParameter = {2, 2, 2, 2};
  const chaps::AttributeList kPublicKeyAttributes = GetFakeAttributes(1);
  const chaps::AttributeList kPrivateKeyAttributes = GetFakeAttributes(2);
  ObjectHandle kResultPublicKeyHandle(20);
  ObjectHandle kResultPrivateKeyHandle(30);
  uint32_t kResultCode = 300;

  std::vector<uint8_t> public_key_attributes_bytes;
  std::vector<uint8_t> private_key_attributes_bytes;
  EXPECT_CALL(
      mock_session_client_,
      GenerateKeyPair(kSlotId, kMechanismType, kMechanismParameter, _, _, _, _))
      .WillOnce(
          DoAll(MoveArg<3>(&public_key_attributes_bytes),
                MoveArg<4>(&private_key_attributes_bytes),
                RunOnceCallback<6>(kResultPublicKeyHandle,
                                   kResultPrivateKeyHandle, kResultCode)));

  base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
  client_.GenerateKeyPair(kSlotId, kMechanismType, kMechanismParameter,
                          kPublicKeyAttributes, kPrivateKeyAttributes,
                          waiter.GetCallback());

  chaps::AttributeList received_public_attributes;
  received_public_attributes.ParseFromArray(public_key_attributes_bytes.data(),
                                            public_key_attributes_bytes.size());
  EXPECT_EQ(received_public_attributes.SerializeAsString(),
            kPublicKeyAttributes.SerializeAsString());
  chaps::AttributeList received_private_attributes;
  received_private_attributes.ParseFromArray(
      private_key_attributes_bytes.data(), private_key_attributes_bytes.size());
  EXPECT_EQ(received_private_attributes.SerializeAsString(),
            kPrivateKeyAttributes.SerializeAsString());

  EXPECT_EQ(waiter.Get<0>(), kResultPublicKeyHandle);
  EXPECT_EQ(waiter.Get<1>(), kResultPrivateKeyHandle);
  EXPECT_EQ(waiter.Get<uint32_t>(), kResultCode);
}

}  // namespace
}  // namespace kcer

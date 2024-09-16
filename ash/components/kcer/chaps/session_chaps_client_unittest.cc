// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/chaps/session_chaps_client.h"

#include <stdint.h>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/chaps/fake_chaps_client.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using ObjectHandle = kcer::SessionChapsClient::ObjectHandle;
using SessionId = kcer::SessionChapsClient::SessionId;
using SlotId = kcer::SessionChapsClient::SlotId;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using testing::_;
using testing::Mock;

namespace kcer {
namespace {

class SessionChapsClientTest : public testing::Test {
 public:
  void SetUp() override {
    if (ash::ChapsClient::Get()) {
      ash::ChapsClient::Shutdown();
    }
    ash::ChapsClient::InitializeFake();
    fake_chaps_client_ =
        static_cast<ash::FakeChapsClient*>(ash::ChapsClient::Get());
  }

  void TearDown() override {
    fake_chaps_client_ = nullptr;
    ash::ChapsClient::Shutdown();
  }

 protected:
  ObjectHandle CreateObject(SlotId slot_id,
                            const std::vector<uint8_t>& attributes) {
    base::test::TestFuture<ObjectHandle, uint32_t> create_waiter;
    client_.CreateObject(slot_id, attributes,
                         /*attempts_left=*/1, create_waiter.GetCallback());
    EXPECT_EQ(create_waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
    return create_waiter.Get<ObjectHandle>();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<ash::FakeChapsClient> fake_chaps_client_ = nullptr;
  SessionChapsClientImpl client_;

  // In the real world the attributes are supposed to be a binary encoding of
  // the chaps::AttributeList proto message. With the current implementation of
  // FakeChapsClient just some binary buffer is good enough.
  const std::vector<uint8_t> kFakeAttrs{1, 1, 1};

  // In the current implementation of FakeChapsClient the query doesn't matter
  // and it will return all attributes anyway.
  const std::vector<uint8_t> kFakeQuery{2, 2, 2};
};

TEST_F(SessionChapsClientTest, CreateAndFetchObject) {
  // Test that CreateObject() can create an object.
  base::test::TestFuture<ObjectHandle, uint32_t> create_waiter;
  client_.CreateObject(SlotId(1), /*attributes=*/kFakeAttrs,
                       /*attempts_left=*/1, create_waiter.GetCallback());
  EXPECT_EQ(create_waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_NE(create_waiter.Get<ObjectHandle>(), ObjectHandle(0));
  ObjectHandle handle = create_waiter.Get<ObjectHandle>();

  // Test that GetAttributeValue() can retrieve attributes from an object and
  // that CreateObject() did forward the attributes correctly.
  base::test::TestFuture<std::vector<uint8_t>, uint32_t> attr_waiter;
  client_.GetAttributeValue(SlotId(1), handle, kFakeQuery,
                            /*attempts_left=*/1, attr_waiter.GetCallback());
  EXPECT_EQ(attr_waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_EQ(attr_waiter.Get<std::vector<uint8_t>>(), kFakeAttrs);
}

TEST_F(SessionChapsClientTest, CreateFindDestroyObject) {
  // Create a test object.
  ObjectHandle handle = CreateObject(SlotId(1), kFakeAttrs);

  // Test that FindObjects() can correctly find the created object.
  base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> find_waiter_1;
  client_.FindObjects(SlotId(1), kFakeQuery,
                      /*attempts_left=*/1, find_waiter_1.GetCallback());
  EXPECT_EQ(find_waiter_1.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_EQ(find_waiter_1.Get<std::vector<ObjectHandle>>(),
            std::vector<ObjectHandle>{handle});

  // Test DestroyObject() can successfully  delete the object.
  base::test::TestFuture<uint32_t> destroy_waiter;
  client_.DestroyObject(SlotId(1), handle,
                        /*attempts_left=*/1, destroy_waiter.GetCallback());
  EXPECT_EQ(destroy_waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);

  // Test that FindObjects() doesn't find the deleted object.
  base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> find_waiter_2;
  client_.FindObjects(SlotId(1), kFakeQuery,
                      /*attempts_left=*/1, find_waiter_2.GetCallback());
  EXPECT_EQ(find_waiter_2.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_EQ(find_waiter_2.Get<std::vector<ObjectHandle>>(),
            std::vector<ObjectHandle>());
}

// Test that GetAttributeValue correctly forwards the arguments to the mojo
// layer and the result back from it.
TEST_F(SessionChapsClientTest, SetGetAttributeValue) {
  // Create a test object.
  ObjectHandle handle = CreateObject(SlotId(1), kFakeAttrs);

  // Create different fake attributes.
  const std::vector<uint8_t> kFakeAttrs2{9, 9, 9, 9};
  ASSERT_NE(kFakeAttrs, kFakeAttrs2);

  // Test that SetAttributeValue() can successfully set attributes on an object.
  base::test::TestFuture<uint32_t> set_waiter;
  client_.SetAttributeValue(SlotId(1), handle, kFakeAttrs2,
                            /*attempts_left=*/1, set_waiter.GetCallback());
  EXPECT_EQ(set_waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);

  // Test that GetAttributeValue() can retrieve attributes from an object and
  // that SetAttributeValue() successfully overwritten the old attributes.
  base::test::TestFuture<std::vector<uint8_t>, uint32_t> get_waiter;
  client_.GetAttributeValue(SlotId(1), handle, kFakeQuery,
                            /*attempts_left=*/1, get_waiter.GetCallback());
  EXPECT_EQ(get_waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_EQ(get_waiter.Get<std::vector<uint8_t>>(), kFakeAttrs2);
}

TEST_F(SessionChapsClientTest, Sign) {
  constexpr uint64_t kFakeMechanismType = 11;
  const std::vector<uint8_t> kFakeMechanismParams = {2, 2, 2};
  const std::vector<uint8_t> kFakeData = {4, 4, 4};
  // In the current implementation of FakeChapsClient, it just returns the
  // original data as signature.
  const std::vector<uint8_t>& kExpectedSignature = kFakeData;

  // In the real world Sign should be called with a handle of a key, but the
  // current implementation of FakeChapsClient only checks that the object
  // exists.
  ObjectHandle handle = CreateObject(SlotId(1), kFakeAttrs);

  // Test that Sign() can be called successfully and that it forwards the data
  // and the signature back correctly.
  base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
  client_.Sign(SlotId(1), kFakeMechanismType, kFakeMechanismParams, handle,
               kFakeData,
               /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), kExpectedSignature);
  EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
}

TEST_F(SessionChapsClientTest, GenerateAndGetKeyPair) {
  constexpr uint64_t kFakeMechanismType = 11;
  const std::vector<uint8_t> kFakeMechanismParams = {2, 2, 2};
  const std::vector<uint8_t> kFakePublicAttrs = {3, 3, 3};
  const std::vector<uint8_t> kFakePrivateAttrs = {4, 4, 4};

  // Test that GenerateKeyPair() can be successfully called.
  base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
  client_.GenerateKeyPair(SlotId(1), kFakeMechanismType, kFakeMechanismParams,
                          kFakePublicAttrs, kFakePrivateAttrs,
                          /*attempts_left=*/1, waiter.GetCallback());
  EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  ObjectHandle pub_key_handle = waiter.Get<0>();
  ObjectHandle priv_key_handle = waiter.Get<1>();
  EXPECT_NE(pub_key_handle, ObjectHandle(0));
  EXPECT_NE(priv_key_handle, ObjectHandle(0));
  EXPECT_NE(pub_key_handle, priv_key_handle);

  // Test that GenerateKeyPair() forwarded the key attributes correctly. Note
  // that in the real world the attributes given to GenerateKeyPair() are more
  // like command arguments and not the actual attributes for the generated
  // keys.

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> get_waiter_1;
  client_.GetAttributeValue(SlotId(1), pub_key_handle, kFakeQuery,
                            /*attempts_left=*/1, get_waiter_1.GetCallback());
  EXPECT_EQ(get_waiter_1.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_EQ(get_waiter_1.Get<std::vector<uint8_t>>(), kFakePublicAttrs);

  base::test::TestFuture<std::vector<uint8_t>, uint32_t> get_waiter_2;
  client_.GetAttributeValue(SlotId(1), priv_key_handle, kFakeQuery,
                            /*attempts_left=*/1, get_waiter_2.GetCallback());
  EXPECT_EQ(get_waiter_2.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  EXPECT_EQ(get_waiter_2.Get<std::vector<uint8_t>>(), kFakePrivateAttrs);
}

// Test that all methods fail if they need to open a session and fail to do so.
TEST_F(SessionChapsClientTest, CannotOpenSessionThenAllMethodsFail) {
  fake_chaps_client_->SimulateOpenSessionError(
      chromeos::PKCS11_CKR_GENERAL_ERROR);

  // GetMechanismList() doesn't require a session, so it wouldn't fail and is
  // not called in the test.

  {
    base::test::TestFuture<ObjectHandle, uint32_t> waiter;
    client_.CreateObject(SlotId(1), /*attributes=*/{},
                         /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  {
    base::test::TestFuture<uint32_t> waiter;
    client_.DestroyObject(SlotId(1), ObjectHandle(0), /*attempts_left=*/1,
                          waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  {
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.GetAttributeValue(SlotId(1), ObjectHandle(1),
                              /*attributes_query=*/{}, /*attempts_left=*/1,
                              waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  {
    base::test::TestFuture<uint32_t> waiter;
    client_.SetAttributeValue(SlotId(1), ObjectHandle(1),
                              /*attributes=*/{},
                              /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  {
    base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
    client_.FindObjects(SlotId(1),
                        /*attributes=*/{},
                        /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  {
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.Sign(SlotId(1), /*mechanism_type=*/0,
                 /*mechanism_parameter=*/{}, /*key_handle=*/ObjectHandle(0),
                 /*data=*/{}, /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  {
    base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
    client_.GenerateKeyPair(SlotId(1), /*mechanism_type=*/0,
                            /*mechanism_parameter=*/{},
                            /*public_attributes=*/{},
                            /*private_attributes=*/{}, /*attempts_left=*/1,
                            waiter.GetCallback());
    EXPECT_NE(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }
}

// Test that the first call to each slot opens a session that can be reused by
// all the other methods.
TEST_F(SessionChapsClientTest, AllMethodsDontReopenSession) {
  // Call a method on two different slots that should open new sessions for
  // them. Also save created objects to use them for other methods.
  std::vector<ObjectHandle> objects_handles(2);
  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<ObjectHandle, uint32_t> waiter;
    client_.CreateObject(SlotId(slot_id), /*attributes=*/{},
                         /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
    objects_handles[slot_id] = waiter.Get<ObjectHandle>();
  }

  // Prevent any new sessions from being opened, as covered by the
  // CannotOpenSessionThenAllMethodsFail test, the methods will fail if they
  // attempt to open new session. They are expected to reuse the existing
  // session.
  fake_chaps_client_->SimulateOpenSessionError(
      chromeos::PKCS11_CKR_GENERAL_ERROR);

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<ObjectHandle, uint32_t> waiter;
    client_.CreateObject(SlotId(slot_id), /*attributes=*/{},
                         /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.GetAttributeValue(SlotId(slot_id), objects_handles[slot_id],
                              /*attributes_query=*/{}, /*attempts_left=*/1,
                              waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<uint32_t> waiter;
    client_.SetAttributeValue(SlotId(slot_id), objects_handles[slot_id],
                              /*attributes=*/{},
                              /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
    client_.FindObjects(SlotId(slot_id),
                        /*attributes=*/{},
                        /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.Sign(SlotId(slot_id), /*mechanism_type=*/0,
                 /*mechanism_parameter=*/{},
                 /*key_handle=*/objects_handles[slot_id],
                 /*data=*/{}, /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
    client_.GenerateKeyPair(
        SlotId(slot_id), /*mechanism_type=*/0,
        /*mechanism_parameter=*/{}, /*public_attributes=*/{},
        /*private_attributes=*/{}, /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }

  for (uint64_t slot_id = 0; slot_id < 2; ++slot_id) {
    base::test::TestFuture<uint32_t> waiter;
    client_.DestroyObject(SlotId(slot_id), objects_handles[slot_id],
                          /*attempts_left=*/1, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chromeos::PKCS11_CKR_OK);
  }
}

// Test that all methods try to open a session multiple times when there's no
// open session at the beginning of each method.
TEST_F(SessionChapsClientTest, AllMethodsTryReopenSession) {
  constexpr int kAttempts = 5;
  fake_chaps_client_->SimulateOpenSessionError(
      chromeos::PKCS11_CKR_GENERAL_ERROR);

  EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), 0);

  {
    base::test::TestFuture<ObjectHandle, uint32_t> waiter;
    client_.CreateObject(SlotId(1), /*attributes=*/{}, kAttempts,
                         waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }

  {
    base::test::TestFuture<uint32_t> waiter;
    client_.DestroyObject(SlotId(1), ObjectHandle(1), kAttempts,
                          waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }

  {
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.GetAttributeValue(SlotId(1), ObjectHandle(1),
                              /*attributes_query=*/{}, kAttempts,
                              waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }

  {
    base::test::TestFuture<uint32_t> waiter;
    client_.SetAttributeValue(SlotId(1), ObjectHandle(1),
                              /*attributes=*/{}, kAttempts,
                              waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }

  {
    base::test::TestFuture<std::vector<ObjectHandle>, uint32_t> waiter;
    client_.FindObjects(SlotId(1),
                        /*attributes=*/{}, kAttempts, waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }

  {
    base::test::TestFuture<std::vector<uint8_t>, uint32_t> waiter;
    client_.Sign(SlotId(1), /*mechanism_type=*/0,
                 /*mechanism_parameter=*/{}, /*key_handle=*/ObjectHandle(0),
                 /*data=*/{}, kAttempts, waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }

  {
    base::test::TestFuture<ObjectHandle, ObjectHandle, uint32_t> waiter;
    client_.GenerateKeyPair(
        SlotId(1), /*mechanism_type=*/0,
        /*mechanism_parameter=*/{}, /*public_attributes=*/{},
        /*private_attributes=*/{}, kAttempts, waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    EXPECT_EQ(fake_chaps_client_->GetAndResetOpenSessionCounter(), kAttempts);
  }
}

}  // namespace
}  // namespace kcer

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/chaps_service_ash.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/chaps/mock_chaps_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace crosapi {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

class ChapsServiceAshTest : public ::testing::Test {
 protected:
  // This instance will be returned from ChapsClient::Get().
  std::unique_ptr<ash::MockChapsClient> dbus_client_ =
      std::make_unique<ash::MockChapsClient>();
  ChapsServiceAsh service_;
};

// Test that GetSlotList correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, GetSlotList) {
  bool token_present = true;
  const std::vector<uint64_t> slot_list = {1, 2, 3};
  const uint32_t result_code = 111;

  {
    EXPECT_CALL(*dbus_client_, GetSlotList(token_present, _))
        .WillOnce(RunOnceCallback<1>(slot_list, result_code));

    base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
    service_.GetSlotList(/*token_present=*/token_present, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), slot_list);
    EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
  }

  token_present = false;

  {
    EXPECT_CALL(*dbus_client_, GetSlotList(token_present, _))
        .WillOnce(RunOnceCallback<1>(slot_list, result_code));

    base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
    service_.GetSlotList(/*token_present=*/token_present, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), slot_list);
    EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
  }
}

// Test that GetMechanismList correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, GetMechanismList) {
  const uint64_t slot_id = 111;
  const std::vector<uint64_t> mechanism_list = {2, 2, 2};
  const uint32_t result_code = 444;

  EXPECT_CALL(*dbus_client_, GetMechanismList(slot_id, _))
      .WillOnce(RunOnceCallback<1>(mechanism_list, result_code));

  base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
  service_.GetMechanismList(slot_id, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), mechanism_list);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that OpenSession correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, OpenSession) {
  const uint64_t slot_id = 111;
  const uint64_t flags = 222;
  const uint64_t session_id = 333;
  const uint32_t result_code = 444;

  EXPECT_CALL(*dbus_client_, OpenSession(slot_id, flags, _))
      .WillOnce(RunOnceCallback<2>(session_id, result_code));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  service_.OpenSession(slot_id, flags, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint64_t>(), session_id);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that CloseSession correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, CloseSession) {
  const uint64_t session_id = 111;
  const uint32_t result_code = 222;

  EXPECT_CALL(*dbus_client_, CloseSession(session_id, _))
      .WillOnce(RunOnceCallback<1>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.CloseSession(session_id, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that CreateObject correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, CreateObject) {
  const uint64_t session_id = 111;
  const std::vector<uint8_t> attributes = {2, 2, 2};
  const uint64_t new_object_handle = 333;
  const uint32_t result_code = 444;

  EXPECT_CALL(*dbus_client_, CreateObject(session_id, attributes, _))
      .WillOnce(RunOnceCallback<2>(new_object_handle, result_code));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  service_.CreateObject(session_id, attributes, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint64_t>(), new_object_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DestroyObject correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, DestroyObject) {
  const uint64_t session_id = 111;
  const uint64_t object_handle = 222;
  const uint32_t result_code = 333;

  EXPECT_CALL(*dbus_client_, DestroyObject(session_id, object_handle, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.DestroyObject(session_id, object_handle, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that GetAttributeValue correctly forwards the arguments to the dbus
// layer and the result back from it.
TEST_F(ChapsServiceAshTest, GetAttributeValue) {
  const uint64_t session_id = 111;
  const uint64_t object_handle = 222;
  const std::vector<uint8_t> attrs_in = {3, 3, 3};
  const std::vector<uint8_t> attrs_out = {4, 4, 4};
  const uint32_t result_code = 555;

  EXPECT_CALL(*dbus_client_,
              GetAttributeValue(session_id, object_handle, attrs_in, _))
      .WillOnce(RunOnceCallback<3>(attrs_out, result_code));

  base::test::TestFuture<const std::vector<uint8_t>&, uint32_t> waiter;
  service_.GetAttributeValue(session_id, object_handle, attrs_in,
                             waiter.GetCallback());

  EXPECT_EQ(waiter.Get<std::vector<uint8_t>>(), attrs_out);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that SetAttributeValue correctly forwards the arguments to the dbus
// layer and the result back from it.
TEST_F(ChapsServiceAshTest, SetAttributeValue) {
  const uint64_t session_id = 111;
  const uint64_t object_handle = 222;
  const std::vector<uint8_t> attributes = {3, 3, 3};
  const uint32_t result_code = 444;

  EXPECT_CALL(*dbus_client_,
              SetAttributeValue(session_id, object_handle, attributes, _))
      .WillOnce(RunOnceCallback<3>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.SetAttributeValue(session_id, object_handle, attributes,
                             waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that FindObjectsInit correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, FindObjectsInit) {
  const uint64_t session_id = 111;
  const std::vector<uint8_t> attributes = {2, 2, 2};
  const uint32_t result_code = 333;

  EXPECT_CALL(*dbus_client_, FindObjectsInit(session_id, attributes, _))
      .WillOnce(RunOnceCallback<2>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.FindObjectsInit(session_id, attributes, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that FindObjects correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, FindObjects) {
  const uint64_t session_id = 111;
  const uint64_t max_object_count = 222;
  const std::vector<uint64_t> object_list = {3, 3, 3};
  const uint32_t result_code = 444;

  EXPECT_CALL(*dbus_client_, FindObjects(session_id, max_object_count, _))
      .WillOnce(RunOnceCallback<2>(object_list, result_code));

  base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
  service_.FindObjects(session_id, max_object_count, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<std::vector<uint64_t>>(), object_list);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that FindObjectsFinal correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, FindObjectsFinal) {
  const uint64_t session_id = 111;
  const uint32_t result_code = 222;

  EXPECT_CALL(*dbus_client_, FindObjectsFinal(session_id, _))
      .WillOnce(RunOnceCallback<1>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.FindObjectsFinal(session_id, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that EncryptInit correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, EncryptInit) {
  const uint64_t session_id = 111;
  const uint64_t mechanism_type = 222;
  const std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  const uint64_t key_handle = 444;
  const uint64_t result_code = 555;

  EXPECT_CALL(*dbus_client_, EncryptInit(session_id, mechanism_type,
                                         mechanism_parameter, key_handle, _))
      .WillOnce(RunOnceCallback<4>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.EncryptInit(session_id, mechanism_type, mechanism_parameter,
                       key_handle, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that Encrypt correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, Encrypt) {
  const uint64_t session_id = 11;
  const std::vector<uint8_t> data = {2, 2, 2};
  const uint64_t max_out_length = 33;
  const uint64_t actual_out_length = 44;
  const std::vector<uint8_t> out_data = {5, 5, 5};
  const uint64_t result_code = 66;

  EXPECT_CALL(*dbus_client_, Encrypt(session_id, data, max_out_length, _))
      .WillOnce(RunOnceCallback<3>(actual_out_length, out_data, result_code));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  service_.Encrypt(session_id, data, max_out_length, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<0>(), actual_out_length);
  EXPECT_EQ(waiter.Get<1>(), out_data);
  EXPECT_EQ(waiter.Get<2>(), result_code);
}

// Test that DecryptInit correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, DecryptInit) {
  const uint64_t session_id = 111;
  const uint64_t mechanism_type = 222;
  const std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  const uint64_t key_handle = 444;
  const uint64_t result_code = 555;

  EXPECT_CALL(*dbus_client_, DecryptInit(session_id, mechanism_type,
                                         mechanism_parameter, key_handle, _))
      .WillOnce(RunOnceCallback<4>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.DecryptInit(session_id, mechanism_type, mechanism_parameter,
                       key_handle, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that Decrypt correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, Decrypt) {
  const uint64_t session_id = 11;
  const std::vector<uint8_t> data = {2, 2, 2};
  const uint64_t max_out_length = 33;
  const uint64_t actual_out_length = 44;
  const std::vector<uint8_t> out_data = {5, 5, 5};
  const uint64_t result_code = 66;

  EXPECT_CALL(*dbus_client_, Decrypt(session_id, data, max_out_length, _))
      .WillOnce(RunOnceCallback<3>(actual_out_length, out_data, result_code));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  service_.Decrypt(session_id, data, max_out_length, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<0>(), actual_out_length);
  EXPECT_EQ(waiter.Get<1>(), out_data);
  EXPECT_EQ(waiter.Get<2>(), result_code);
}

// Test that SignInit correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, SignInit) {
  const uint64_t session_id = 111;
  const uint64_t mechanism_type = 222;
  const std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  const uint64_t key_handle = 444;
  const uint64_t result_code = 555;

  EXPECT_CALL(*dbus_client_, SignInit(session_id, mechanism_type,
                                      mechanism_parameter, key_handle, _))
      .WillOnce(RunOnceCallback<4>(result_code));

  base::test::TestFuture<uint32_t> waiter;
  service_.SignInit(session_id, mechanism_type, mechanism_parameter, key_handle,
                    waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that Sign correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, Sign) {
  const uint64_t session_id = 11;
  const std::vector<uint8_t> data = {2, 2, 2};
  const uint64_t max_out_length = 33;
  const uint64_t actual_out_length = 44;
  const std::vector<uint8_t> signature = {5, 5, 5};
  const uint64_t result_code = 66;

  EXPECT_CALL(*dbus_client_, Sign(session_id, data, max_out_length, _))
      .WillOnce(RunOnceCallback<3>(actual_out_length, signature, result_code));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  service_.Sign(session_id, data, max_out_length, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<0>(), actual_out_length);
  EXPECT_EQ(waiter.Get<1>(), signature);
  EXPECT_EQ(waiter.Get<2>(), result_code);
}

// Test that GenerateKeyPair correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, GenerateKeyPair) {
  const uint64_t session_id = 111;
  const uint64_t mechanism_type = 222;
  const std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  const std::vector<uint8_t> public_attributes = {4, 4, 4};
  const std::vector<uint8_t> private_attributes = {5, 5, 5};
  const uint64_t public_key_handle = 777;
  const uint64_t private_key_handle = 888;
  const uint32_t result_code = 999;

  EXPECT_CALL(*dbus_client_,
              GenerateKeyPair(session_id, mechanism_type, mechanism_parameter,
                              public_attributes, private_attributes, _))
      .WillOnce(RunOnceCallback<5>(public_key_handle, private_key_handle,
                                   result_code));

  base::test::TestFuture<uint64_t, uint64_t, uint32_t> waiter;
  service_.GenerateKeyPair(session_id, mechanism_type, mechanism_parameter,
                           public_attributes, private_attributes,
                           waiter.GetCallback());

  EXPECT_EQ(waiter.Get<0>(), public_key_handle);
  EXPECT_EQ(waiter.Get<1>(), private_key_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that WrapKey correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, WrapKey) {
  uint64_t session_id = 11;
  uint64_t mechanism_type = 22;
  std::vector<uint8_t> mechanism_parameter = {3, 3, 3};
  uint64_t wrapping_key_handle = 44;
  uint64_t key_handle = 55;
  uint64_t max_out_length = 66;
  uint64_t actual_out_length = 77;
  std::vector<uint8_t> out_wrapped_key = {8, 8, 8};
  uint32_t result_code = 99;

  EXPECT_CALL(*dbus_client_,
              WrapKey(session_id, mechanism_type, mechanism_parameter,
                      wrapping_key_handle, key_handle, max_out_length, _))
      .WillOnce(
          RunOnceCallback<6>(actual_out_length, out_wrapped_key, result_code));

  base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
      waiter;
  service_.WrapKey(session_id, mechanism_type, mechanism_parameter,
                   wrapping_key_handle, key_handle, max_out_length,
                   waiter.GetCallback());

  EXPECT_EQ(waiter.Get<0>(), actual_out_length);
  EXPECT_EQ(waiter.Get<1>(), out_wrapped_key);
  EXPECT_EQ(waiter.Get<2>(), result_code);
}

// Test that UnwrapKey correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, UnwrapKey) {
  const uint64_t session_id = 11;
  const uint64_t mechanism_type = 22;
  const std::vector<uint8_t> mechanism_parameter = {3, 3};
  const uint64_t wrapping_key_handle = 44;
  const std::vector<uint8_t> wrapped_key = {5, 5};
  const std::vector<uint8_t> attributes = {6, 6};
  const uint64_t key_handle = 77;
  const uint32_t result_code = 88;

  EXPECT_CALL(*dbus_client_,
              UnwrapKey(session_id, mechanism_type, mechanism_parameter,
                        wrapping_key_handle, wrapped_key, attributes, _))
      .WillOnce(RunOnceCallback<6>(key_handle, result_code));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  service_.UnwrapKey(session_id, mechanism_type, mechanism_parameter,
                     wrapping_key_handle, wrapped_key, attributes,
                     waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint64_t>(), key_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that DeriveKey correctly forwards the arguments to the dbus layer
// and the result back from it.
TEST_F(ChapsServiceAshTest, DeriveKey) {
  const uint64_t session_id = 11;
  const uint64_t mechanism_type = 22;
  const std::vector<uint8_t> mechanism_parameter = {3, 3};
  const uint64_t base_key_handle = 44;
  const std::vector<uint8_t> attributes = {6, 6};
  const uint64_t key_handle = 77;
  const uint32_t result_code = 88;

  EXPECT_CALL(*dbus_client_,
              DeriveKey(session_id, mechanism_type, mechanism_parameter,
                        base_key_handle, attributes, _))
      .WillOnce(RunOnceCallback<5>(key_handle, result_code));

  base::test::TestFuture<uint64_t, uint32_t> waiter;
  service_.DeriveKey(session_id, mechanism_type, mechanism_parameter,
                     base_key_handle, attributes, waiter.GetCallback());

  EXPECT_EQ(waiter.Get<uint64_t>(), key_handle);
  EXPECT_EQ(waiter.Get<uint32_t>(), result_code);
}

// Test that all methods correctly fail when dbus is not initialized.
TEST_F(ChapsServiceAshTest, AllMethodsDbusClientIsNull) {
  dbus_client_.reset();
  {
    base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
    service_.GetSlotList(/*token_present=*/true, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
    service_.GetMechanismList(/*slot_id=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, uint32_t> waiter;
    service_.OpenSession(/*slot_id=*/0, /*flags=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.CloseSession(/*session_id=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, uint32_t> waiter;
    service_.CreateObject(/*session_id=*/0, /*attributes=*/{},
                          waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.DestroyObject(/*session_id=*/0, /*object_handle=*/0,
                           waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<const std::vector<uint8_t>&, uint32_t> waiter;
    service_.GetAttributeValue(/*session_id=*/0, /*object_handle=*/0,
                               /*attributes=*/{}, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.SetAttributeValue(/*session_id=*/0, /*object_handle=*/0,
                               /*attributes=*/{}, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.FindObjectsInit(/*session_id=*/0, /*attributes=*/{},
                             waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<const std::vector<uint64_t>&, uint32_t> waiter;
    service_.FindObjects(/*session_id=*/0, /*max_object_count=*/0,
                         waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.FindObjectsFinal(/*session_id=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.EncryptInit(/*session_id=*/0, /*mechanism_type=*/0,
                         /*mechanism_parameter=*/{},
                         /*key_handle=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
        waiter;
    service_.Encrypt(/*session_id=*/0,
                     /*data=*/{},
                     /*max_out_length=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.DecryptInit(/*session_id=*/0, /*mechanism_type=*/0,
                         /*mechanism_parameter=*/{},
                         /*key_handle=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
        waiter;
    service_.Decrypt(/*session_id=*/0,
                     /*data=*/{},
                     /*max_out_length=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint32_t> waiter;
    service_.SignInit(/*session_id=*/0, /*mechanism_type=*/0,
                      /*mechanism_parameter=*/{},
                      /*key_handle=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, const std::vector<uint8_t>&, uint32_t>
        waiter;
    service_.Sign(/*session_id=*/0,
                  /*data=*/{},
                  /*max_out_length=*/0, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, uint64_t, uint32_t> waiter;
    service_.GenerateKeyPair(/*session_id=*/0, /*mechanism_type=*/0,
                             /*mechanism_parameter=*/{},
                             /*public_attributes=*/{},
                             /*private_attributes=*/{}, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, uint32_t> waiter;
    service_.UnwrapKey(/*session_id=*/0, /*mechanism_type=*/0,
                       /*mechanism_parameter=*/{},
                       /*wrapping_key_handle=*/0,
                       /*wrapped_key=*/{},
                       /*attributes=*/{}, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  {
    base::test::TestFuture<uint64_t, uint32_t> waiter;
    service_.DeriveKey(/*session_id=*/0, /*mechanism_type=*/0,
                       /*mechanism_parameter=*/{},
                       /*base_key_handle=*/0,
                       /*attributes=*/{}, waiter.GetCallback());
    EXPECT_EQ(waiter.Get<uint32_t>(), chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
}

}  // namespace
}  // namespace crosapi

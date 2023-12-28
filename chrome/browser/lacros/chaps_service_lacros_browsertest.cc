// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using crosapi::mojom::ChapsService;
using crosapi::mojom::Crosapi;

namespace {

class ChapsLacrosBrowserTest : public InProcessBrowserTest {
 public:
 protected:
  bool ShouldSkip() {
    chromeos::LacrosService* service = chromeos::LacrosService::Get();
    const int required_version = static_cast<int>(
        Crosapi::MethodMinVersions::kBindChapsServiceMinVersion);
    if ((service->GetInterfaceVersion<Crosapi>() < required_version) &&
        !service->IsSupported<ChapsService>()) {
      return true;
    }
    return false;
  }

  mojo::Remote<ChapsService>& chaps_remote() {
    return chromeos::LacrosService::Get()->GetRemote<ChapsService>();
  }
};

// Test that all mojom::ChapsService methods are callable from Lacros. All of
// them are expected to fail in the browser test because Ash-on-Linux doesn't
// have a way to actually talk to Chaps (the FakeChapsClient is used instead and
// at the moment it always returns errors).
IN_PROC_BROWSER_TEST_F(ChapsLacrosBrowserTest, AllMethodsAreCallable) {
  if (ShouldSkip()) {
    GTEST_SKIP();
  }

  crosapi::mojom::ChapsServiceAsyncWaiter async_waiter(chaps_remote().get());

  {
    std::vector<uint64_t> out_slot_list;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.GetSlotList(/*token_present=*/true, &out_slot_list,
                             &out_result);
    EXPECT_TRUE(out_slot_list.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    std::vector<uint64_t> out_mechanism_list;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.GetMechanismList(/*token_present=*/true, &out_mechanism_list,
                                  &out_result);
    EXPECT_TRUE(out_mechanism_list.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t out_session_id = static_cast<uint64_t>(-1);
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.OpenSession(/*slot_id=*/0, /*flags=*/0, &out_session_id,
                             &out_result);
    EXPECT_EQ(out_session_id, 0u);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.CloseSession(/*session_id=*/0, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t out_new_object_handle = static_cast<uint64_t>(-1);
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.CreateObject(
        /*session_id=*/0, /*attributes=*/{}, &out_new_object_handle,
        &out_result);
    EXPECT_EQ(out_new_object_handle, 0u);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.DestroyObject(
        /*session_id=*/0, /*object_handle=*/0, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    std::vector<uint8_t> out_attributes_out;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.GetAttributeValue(
        /*session_id=*/0, /*object_handle=*/0,
        /*attributes_in=*/{}, &out_attributes_out, &out_result);
    EXPECT_TRUE(out_attributes_out.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.SetAttributeValue(
        /*session_id=*/0, /*object_handle=*/0,
        /*attributes=*/{}, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.FindObjectsInit(
        /*session_id=*/0, /*attributes=*/{}, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    std::vector<uint64_t> out_object_list;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.FindObjects(
        /*session_id=*/0, /*max_object_count=*/0, &out_object_list,
        &out_result);
    EXPECT_TRUE(out_object_list.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.FindObjectsFinal(/*session_id=*/0, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.EncryptInit(
        /*session_id=*/0, /*mechanism_type=*/0, /*mechanism_parameter=*/{},
        /*key_handle=*/0, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t actual_out_length = static_cast<uint64_t>(-1);
    std::vector<uint8_t> data;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.Encrypt(
        /*session_id=*/0, /*data=*/{}, /*max_out_length=*/0, &actual_out_length,
        &data, &out_result);
    EXPECT_EQ(actual_out_length, 0u);
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.DecryptInit(
        /*session_id=*/0, /*mechanism_type=*/0, /*mechanism_parameter=*/{},
        /*key_handle=*/0, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t actual_out_length = static_cast<uint64_t>(-1);
    std::vector<uint8_t> data;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.Decrypt(
        /*session_id=*/0, /*data=*/{}, /*max_out_length=*/0, &actual_out_length,
        &data, &out_result);
    EXPECT_EQ(actual_out_length, 0u);
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.SignInit(
        /*session_id=*/0, /*mechanism_type=*/0, /*mechanism_parameter=*/{},
        /*key_handle=*/0, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t actual_out_length = static_cast<uint64_t>(-1);
    std::vector<uint8_t> signature;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.Sign(
        /*session_id=*/0, /*data=*/{}, /*max_out_length=*/0, &actual_out_length,
        &signature, &out_result);
    EXPECT_EQ(actual_out_length, 0u);
    EXPECT_TRUE(signature.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t out_public_key_handle = static_cast<uint64_t>(-1);
    uint64_t out_private_key_handle = static_cast<uint64_t>(-1);
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.GenerateKeyPair(
        /*session_id=*/0, /*mechanism_type=*/0,
        /*mechanism_parameter=*/{},
        /*public_attributes=*/{},
        /*private_attributes=*/{}, &out_public_key_handle,
        &out_private_key_handle, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t actual_out_length = static_cast<uint64_t>(-1);
    std::vector<uint8_t> wrapped_key;
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.WrapKey(
        /*session_id=*/0, /*mechanism_type=*/0, /*mechanism_parameter=*/{},
        /*wrapping_key_handle=*/0, /*key_handle=*/0, /*max_out_length=*/0,
        &actual_out_length, &wrapped_key, &out_result);
    EXPECT_EQ(actual_out_length, 0u);
    EXPECT_TRUE(wrapped_key.empty());
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t out_key_handle = static_cast<uint64_t>(-1);
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.UnwrapKey(
        /*session_id=*/0, /*mechanism_type=*/0,
        /*mechanism_parameter=*/{},
        /*wrapping_key_handle=*/0,
        /*wrapped_key=*/{}, /*attributes=*/{}, &out_key_handle, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
  {
    uint64_t out_key_handle = static_cast<uint64_t>(-1);
    uint32_t out_result = static_cast<uint32_t>(-1);
    async_waiter.DeriveKey(
        /*session_id=*/0, /*mechanism_type=*/0,
        /*mechanism_parameter=*/{},
        /*base_key_handle=*/0,
        /*attributes=*/{}, &out_key_handle, &out_result);
    EXPECT_EQ(out_result, chromeos::PKCS11_CKR_GENERAL_ERROR);
  }
}

}  // namespace

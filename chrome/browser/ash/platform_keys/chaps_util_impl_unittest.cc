// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/chaps_util_impl.h"

#include <pkcs11t.h>
#include <secmodt.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/ash/platform_keys/chaps_slot_session.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace platform_keys {
namespace {

using ::testing::Optional;

const size_t kKeySizeBits = 2048;

// TODO(b/202374261): Move these into a shared header.
// Signals to chaps that a generated key should be software-backed.
constexpr CK_ATTRIBUTE_TYPE kForceSoftwareAttribute = CKA_VENDOR_DEFINED + 4;
// Chaps sets this for keys that are software-backed.
constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;

// Holds PKCS#11 attributes passed by the code under test.
struct ObjectAttributes {
  ObjectAttributes() = default;
  ~ObjectAttributes() = default;

  ObjectAttributes(const ObjectAttributes& other) = default;
  ObjectAttributes& operator=(const ObjectAttributes& other) = default;

  ObjectAttributes(ObjectAttributes&& other) = default;
  ObjectAttributes& operator=(ObjectAttributes&& other) = default;

  absl::optional<CK_BBOOL> cka_token;
  absl::optional<CK_BBOOL> cka_private;
  absl::optional<CK_BBOOL> cka_verify;
  absl::optional<CK_ULONG> cka_modulus_bits;
  absl::optional<std::vector<CK_BYTE>> cka_public_exponent;
  absl::optional<CK_BBOOL> cka_sensitive;
  absl::optional<CK_BBOOL> cka_extractable;
  absl::optional<CK_BBOOL> cka_sign;
  absl::optional<CK_BBOOL> force_software;

  static ObjectAttributes ParseFrom(CK_ATTRIBUTE_PTR attributes,
                                    CK_ULONG attributes_count) {
    ObjectAttributes result;

    for (CK_ULONG i = 0; i < attributes_count; ++i) {
      const CK_ATTRIBUTE& attribute = attributes[i];
      switch (attribute.type) {
        case CKA_TOKEN:
          result.cka_token = GetCkBBool(attribute, "CKA_TOKEN");
          break;
        case CKA_PRIVATE:
          result.cka_private = GetCkBBool(attribute, "CKA_PRIVATE");
          break;
        case CKA_VERIFY:
          result.cka_verify = GetCkBBool(attribute, "CKA_VERIFY");
          break;
        case CKA_MODULUS_BITS:
          result.cka_modulus_bits = GetCkULong(attribute, "CKA_MODULUS_BITS");
          break;
        case CKA_PUBLIC_EXPONENT:
          result.cka_public_exponent = GetBytes(attribute);
          break;
        case CKA_SENSITIVE:
          result.cka_sensitive = GetCkBBool(attribute, "CKA_SENSITIVE");
          break;
        case CKA_EXTRACTABLE:
          result.cka_extractable = GetCkBBool(attribute, "CKA_EXTRACTABLE");
          break;
        case CKA_SIGN:
          result.cka_sign = GetCkBBool(attribute, "CKA_SIGN");
          break;
        case kForceSoftwareAttribute:
          result.force_software =
              GetCkBBool(attribute, "kForceSoftwareAttribute");
          break;
      }
    }
    return result;
  }

  static absl::optional<CK_BBOOL> GetCkBBool(const CK_ATTRIBUTE& attribute,
                                             const char* attribute_name) {
    if (attribute.ulValueLen < sizeof(CK_BBOOL)) {
      ADD_FAILURE() << "Size to small for CK_BBOOL for attribute "
                    << attribute_name << ": " << attribute.ulValueLen;
      return absl::nullopt;
    }
    CK_BBOOL value;
    memcpy(&value, attribute.pValue, sizeof(CK_BBOOL));
    return value;
  }

  static absl::optional<CK_ULONG> GetCkULong(const CK_ATTRIBUTE& attribute,
                                             const char* attribute_name) {
    if (attribute.ulValueLen < sizeof(CK_ULONG)) {
      ADD_FAILURE() << "Size to small for CK_ULONG for attribute "
                    << attribute_name << ": " << attribute.ulValueLen;
      return absl::nullopt;
    }
    CK_ULONG value;
    memcpy(&value, attribute.pValue, sizeof(CK_ULONG));
    return value;
  }

  static absl::optional<std::vector<CK_BYTE>> GetBytes(
      const CK_ATTRIBUTE& attribute) {
    std::vector<CK_BYTE> result(attribute.ulValueLen);
    memcpy(result.data(), attribute.pValue, result.size());
    return result;
  }
};

// Holds
// - flags triggering how FakeChapsSlotSession should behave.
// - data passed by the code under test to FakeChapsSlotSessionFactory and
// FakeChapsSlotSession.
struct PassedData {
  // Controls whether ChapsSlotSessionFactory::CreateChapsSlotSession succeeds.
  bool factory_success = true;
  // Assigns results to operations. The key is the operation index, i.e. the
  // sequence number of an operation performed on the ChapsSlotSession.
  // The value is the operation result. CKR_INVALID_SESSION_HANDLE and
  // CKR_SESSION_CLOSED have special meaning.
  std::map<int, CK_RV> operation_results;
  // If set to false, calls to ChapsSlotSession::ReopenSession will fail.
  bool reopen_session_success = true;

  // Counts how often teh code under test called
  // ChapsSlotSession::ReopenSession.
  int reopen_session_call_count = 0;

  // The slot_id passed into FakeChapsSlotSessionFactory.
  absl::optional<CK_SLOT_ID> slot_id;
  // Attributes passed for the public key template to GenerateKeyPair.
  ObjectAttributes public_key_gen_attributes;
  // Attributes passed for the private key template to GenerateKeyPair.
  ObjectAttributes private_key_gen_attributes;
  // The data passed into FakeChapsSlotSession::SetAttributeValue for the CKA_ID
  // attribute of the public key. Empty if SetAttributeValue was never called
  // for that attribute.
  std::vector<uint8_t> public_key_cka_id;
  // The data passed into FakeChapsSlotSession::SetAttributeValue for the CKA_ID
  // attribute of the private key. Empty if SetAttributeValue was never called
  // for that attribute.
  std::vector<uint8_t> private_key_cka_id;
};

// The FakeChapsSlotSession actually generating a key pair on a NSS slot.
// This is useful so it's possible to test whether the CKA_ID that the code
// under test would assign matches the CKA_ID that NSS computed for the key.
class FakeChapsSlotSession : public ChapsSlotSession {
 public:
  explicit FakeChapsSlotSession(PK11SlotInfo* slot, PassedData* passed_data)
      : slot_(slot), passed_data_(passed_data) {}
  ~FakeChapsSlotSession() override = default;

  bool ReopenSession() override {
    ++passed_data_->reopen_session_call_count;

    // The code under test should only call this if it was given an indication
    // that the session handle it uses is not valid anymore.
    EXPECT_FALSE(session_ok_);
    if (passed_data_->reopen_session_success) {
      session_ok_ = true;
      return true;
    }
    return false;
  }

  CK_RV GenerateKeyPair(CK_MECHANISM_PTR pMechanism,
                        CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                        CK_ULONG ulPublicKeyAttributeCount,
                        CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                        CK_ULONG ulPrivateKeyAttributeCount,
                        CK_OBJECT_HANDLE_PTR phPublicKey,
                        CK_OBJECT_HANDLE_PTR phPrivateKey) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK)
      return configured_result;

    passed_data_->public_key_gen_attributes = ObjectAttributes::ParseFrom(
        pPublicKeyTemplate, ulPublicKeyAttributeCount);
    passed_data_->private_key_gen_attributes = ObjectAttributes::ParseFrom(
        pPrivateKeyTemplate, ulPrivateKeyAttributeCount);

    crypto::ScopedSECKEYPublicKey public_key;
    crypto::ScopedSECKEYPrivateKey private_key;
    EXPECT_TRUE(crypto::GenerateRSAKeyPairNSS(slot_, kKeySizeBits,
                                              /*permanent=*/true, &public_key,
                                              &private_key));
    *phPublicKey = public_key->pkcs11ID;
    public_key_handle_ = public_key->pkcs11ID;
    *phPrivateKey = private_key->pkcs11ID;
    private_key_handle_ = private_key->pkcs11ID;

    // Remember the modulus
    SECItem* modulus = &(public_key->u.rsa.modulus);
    public_key_modulus_.assign(modulus->data, modulus->data + modulus->len);
    return CKR_OK;
  }

  CK_RV GetAttributeValue(CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK)
      return configured_result;

    if (hObject == public_key_handle_) {
      const size_t kModulusBytes = kKeySizeBits / 8;
      if (ulCount != 1 || pTemplate[0].type != CKA_MODULUS)
        return CKR_ATTRIBUTE_TYPE_INVALID;
      if (pTemplate[0].ulValueLen < kModulusBytes)
        return CKR_BUFFER_TOO_SMALL;
      memcpy(pTemplate[0].pValue, public_key_modulus_.data(), kModulusBytes);
      return CKR_OK;
    }
    if (hObject == private_key_handle_) {
      if (ulCount != 1 || pTemplate[0].type != kKeyInSoftware)
        return CKR_ATTRIBUTE_TYPE_INVALID;
      const CK_BBOOL key_in_software_value = true;
      if (pTemplate[0].ulValueLen < sizeof(key_in_software_value))
        return CKR_BUFFER_TOO_SMALL;
      memcpy(pTemplate[0].pValue, &key_in_software_value,
             sizeof(key_in_software_value));
      return CKR_OK;
    }
    return CKR_OBJECT_HANDLE_INVALID;
  }

  CK_RV SetAttributeValue(CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) override {
    EXPECT_TRUE(session_ok_);
    CK_RV configured_result = ApplyConfiguredResult();
    if (configured_result != CKR_OK)
      return configured_result;

    if (ulCount != 1 || pTemplate[0].type != CKA_ID)
      return CKR_ATTRIBUTE_TYPE_INVALID;

    uint8_t* data = reinterpret_cast<uint8_t*>(pTemplate[0].pValue);
    size_t length = pTemplate[0].ulValueLen;
    if (hObject == public_key_handle_) {
      passed_data_->public_key_cka_id.assign(data, data + length);
      return CKR_OK;
    } else if (hObject == private_key_handle_) {
      passed_data_->private_key_cka_id.assign(data, data + length);
      return CKR_OK;
    }
    return CKR_OBJECT_HANDLE_INVALID;
  }

 private:
  // Applies a result configured for the current operation, if any.
  CK_RV ApplyConfiguredResult() {
    int cur_operation = operation_count_;
    ++operation_count_;

    auto operation_result = passed_data_->operation_results.find(cur_operation);
    if (operation_result == passed_data_->operation_results.end())
      return CKR_OK;
    CK_RV result = operation_result->second;
    // CKR_SESSION_HANDLE_INVALID and CKR_SESSION_CLOSED have a special meaning
    // - also flag that the session handle is not usable (until the next call to
    // ReopenSession).
    if (result == CKR_SESSION_HANDLE_INVALID || result == CKR_SESSION_CLOSED)
      session_ok_ = false;
    return result;
  }

  // Keeps track of how many operations were already performed. Used to keep
  // track of the operation sequence number for PassedData::operation_results.
  int operation_count_ = 0;
  // If false, the session is not usable until ReopenSession has been called.
  bool session_ok_ = true;

  // Unowned.
  PK11SlotInfo* const slot_;
  // Unowned.
  PassedData* const passed_data_;

  // Cached modulus of the generated public key so GetAttributeValue with
  // CKA_MODULUS is supported.
  std::vector<uint8_t> public_key_modulus_;
  CK_OBJECT_HANDLE public_key_handle_ = CKR_OBJECT_HANDLE_INVALID;
  CK_OBJECT_HANDLE private_key_handle_ = CKR_OBJECT_HANDLE_INVALID;
};

class FakeChapsSlotSessionFactory : public ChapsSlotSessionFactory {
 public:
  FakeChapsSlotSessionFactory(PK11SlotInfo* slot, PassedData* passed_data)
      : slot_(slot), passed_data_(passed_data) {}
  ~FakeChapsSlotSessionFactory() override = default;

  std::unique_ptr<ChapsSlotSession> CreateChapsSlotSession(
      CK_SLOT_ID slot_id) override {
    passed_data_->slot_id = slot_id;
    if (!passed_data_->factory_success)
      return nullptr;
    return std::make_unique<FakeChapsSlotSession>(slot_, passed_data_);
  }

 private:
  // Unowned.
  PK11SlotInfo* const slot_;
  // Unowned.
  PassedData* const passed_data_;
};

class ChapsUtilImplTest : public ::testing::Test {
 public:
  ChapsUtilImplTest() {
    auto chaps_slot_session_factory =
        std::make_unique<FakeChapsSlotSessionFactory>(nss_test_db_.slot(),
                                                      &passed_data_);
    chaps_util_impl_ =
        std::make_unique<ChapsUtilImpl>(std::move(chaps_slot_session_factory));
    chaps_util_impl_->SetIsChapsProvidedSlotForTesting(true);
  }
  ChapsUtilImplTest(const ChapsUtilImplTest&) = delete;
  ChapsUtilImplTest& operator=(const ChapsUtilImplTest&) = delete;
  ~ChapsUtilImplTest() override = default;

 protected:
  crypto::ScopedTestNSSDB nss_test_db_;
  PassedData passed_data_;

  std::unique_ptr<ChapsUtilImpl> chaps_util_impl_;
};

// Returns the CKA_ID of |private_key|. This is the CKA_ID that NSS assigned to
// the private key, and thus it should be the same CKA_ID that ChapsUtil
// attempts to assign to the private and public key.
std::vector<uint8_t> GetExpectedCkaId(SECKEYPrivateKey* private_key) {
  crypto::ScopedSECItem cka_id_secitem(
      PK11_GetLowLevelKeyIDForPrivateKey(private_key));
  uint8_t* cka_id_data = reinterpret_cast<uint8_t*>(cka_id_secitem->data);
  return {cka_id_data, cka_id_data + cka_id_secitem->len};
}

// Successfully generates a software-backed key pair. Also verifies CKA_ID
// assignment.
TEST_F(ChapsUtilImplTest, Success) {
  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  ASSERT_TRUE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));

  // Verify that ChapsUtil passed the correct slot id to the factory.
  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));

  // Verify that ChapsUtil passed the expected attributes
  EXPECT_THAT(passed_data_.public_key_gen_attributes.cka_token,
              Optional(CK_TRUE));
  EXPECT_THAT(passed_data_.public_key_gen_attributes.cka_private,
              Optional(CK_FALSE));
  EXPECT_THAT(passed_data_.public_key_gen_attributes.cka_verify,
              Optional(CK_TRUE));
  EXPECT_THAT(passed_data_.public_key_gen_attributes.cka_modulus_bits,
              Optional((CK_ULONG)2048));
  EXPECT_THAT(passed_data_.public_key_gen_attributes.cka_public_exponent,
              Optional(std::vector<CK_BYTE>{0x01, 0x00, 0x01}));

  EXPECT_THAT(passed_data_.private_key_gen_attributes.cka_token,
              Optional(CK_TRUE));
  EXPECT_THAT(passed_data_.private_key_gen_attributes.cka_private,
              Optional(CK_TRUE));
  EXPECT_THAT(passed_data_.private_key_gen_attributes.cka_sensitive,
              Optional(CK_TRUE));
  EXPECT_THAT(passed_data_.private_key_gen_attributes.cka_extractable,
              Optional(CK_FALSE));
  EXPECT_THAT(passed_data_.private_key_gen_attributes.force_software,
              Optional(CK_TRUE));
  EXPECT_THAT(passed_data_.private_key_gen_attributes.cka_sign,
              Optional(CK_TRUE));

  // Verify that ChapsUtil attempted to assign the correct CKA_ID to the public
  // and private key objects.
  std::vector<uint8_t> expected_cka_id = GetExpectedCkaId(private_key.get());
  EXPECT_EQ(passed_data_.public_key_cka_id, expected_cka_id);
  EXPECT_EQ(passed_data_.private_key_cka_id, expected_cka_id);
}

// The passed slot is not provided by chaps. The operation fails.
TEST_F(ChapsUtilImplTest, NotChapsProvidedSlot) {
  chaps_util_impl_->SetIsChapsProvidedSlotForTesting(false);

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
}

// A ChapsSlotSession can not be created, so the operation fails.
TEST_F(ChapsUtilImplTest, ChapsSlotSessionFactoryFailure) {
  passed_data_.factory_success = false;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));

  // Verify that ChapsUtil passed the correct slot id to the factory.
  EXPECT_EQ(passed_data_.slot_id, PK11_GetSlotID(nss_test_db_.slot()));
}

// A PKCS11 operation fails with a generic failure. The operation fails.
TEST_F(ChapsUtilImplTest, OperationFails) {
  passed_data_.operation_results[0] = CKR_FUNCTION_FAILED;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID. ChapsUtilImpl
// re-opens the session and retries the operation.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenOk) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.reopen_session_success = true;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_TRUE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 1);
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID twice. ChapsUtilImpl
// re-opens the session and retries the operation.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenTwiceOk) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[1] = CKR_SESSION_CLOSED;
  passed_data_.reopen_session_success = true;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_TRUE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 2);
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID many times.
// ChapsUtilImpl gives up attempts to retry after 5 times.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenGivesUp) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[1] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[2] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[3] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[4] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.operation_results[5] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.reopen_session_success = true;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 5);
}

// A PKCS11 operation fails with CKR_SESSION_HANDLE_INVALID and the session can
// not be re-opened. The operation fails.
TEST_F(ChapsUtilImplTest, HandlesInvalidSessionHandle_ReopenFails) {
  passed_data_.operation_results[0] = CKR_SESSION_HANDLE_INVALID;
  passed_data_.reopen_session_success = false;

  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;
  EXPECT_FALSE(chaps_util_impl_->GenerateSoftwareBackedRSAKey(
      nss_test_db_.slot(), kKeySizeBits, &public_key, &private_key));
  EXPECT_EQ(passed_data_.reopen_session_call_count, 1);
}

}  // namespace
}  // namespace platform_keys
}  // namespace ash

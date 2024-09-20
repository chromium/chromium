// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef ASH_COMPONENTS_KCER_CHAPS_HIGH_LEVEL_CHAPS_CLIENT_H_
#define ASH_COMPONENTS_KCER_CHAPS_HIGH_LEVEL_CHAPS_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "ash/components/kcer/attributes.pb.h"
#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace kcer {

// Adds an attribute with the given `type` to `attr_list` and sets the value to
// `data`.
COMPONENT_EXPORT(KCER)
void AddAttribute(chaps::AttributeList& attr_list,
                  chromeos::PKCS11_CK_ATTRIBUTE_TYPE type,
                  base::span<const uint8_t> data);

// Reinterprets the `value` as a sequence of bytes and returns it as a span.
// `T` must be a simple type, i.e. no internal pointers, etc.
// `value` must outlive the returned span.
template <typename T>
COMPONENT_EXPORT(KCER)
base::span<const uint8_t> MakeSpan(T* value) {
  static_assert(std::is_integral_v<T>);
  return base::as_bytes(base::span<T>(value, /*count=*/1u));
}

// The main class to communicate with Chaps. Further simplifies the D-Bus
// protocol (on top of SessionChapsClient):
// * Uses more friendly types for arguments.
// * Tries to guess attribute sizes for GetAttributeValue and handles the
// retrieval of the actual sizes when needed.
// * Adds additional convenience methods.
// If any of the methods fail with a session error (see
// SessionChapsClient::IsSessionError), all ObjectHandle-s become obsolete and
// should be immediately discarded (Chaps could still accept them, but they
// might start referring to different objects).
class HighLevelChapsClient {
 public:
  using GetAttributeValueCallback =
      base::OnceCallback<void(chaps::AttributeList attributes,
                              uint32_t result_code)>;

  // A list of all attributes supported by the GetAttributeValue,
  // SetAttributeValue methods, can be expanded as needed. It's allowed to cast
  // from AttributeId to uint32_t.
  enum class AttributeId : uint32_t {
    kModulus = chromeos::PKCS11_CKA_MODULUS,
    kPublicExponent = chromeos::PKCS11_CKA_PUBLIC_EXPONENT,
    kEcPoint = chromeos::PKCS11_CKA_EC_POINT,
    kPkcs11Id = chromeos::PKCS11_CKA_ID,
    kLabel = chromeos::PKCS11_CKA_LABEL,
    kKeyType = chromeos::PKCS11_CKA_KEY_TYPE,
    kValue = chromeos::PKCS11_CKA_VALUE,
    // Stored on the private key.
    kKeyInSoftware = chaps::kKeyInSoftwareAttribute,
    kKeyPermissions = pkcs11_custom_attributes::kCkaChromeOsKeyPermissions,
    kCertProvisioningId =
        pkcs11_custom_attributes::kCkaChromeOsBuiltinProvisioningProfileId,
  };

  HighLevelChapsClient() = default;
  virtual ~HighLevelChapsClient() = default;

  // PKCS #11 v2.20 section 11.5 page 111.
  virtual void GetMechanismList(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::GetMechanismListCallback callback) = 0;
  // Similar to PKCS #11 v2.20 section 11.7 page 128.
  virtual void CreateObject(
      SessionChapsClient::SlotId slot_id,
      const chaps::AttributeList& attributes,
      SessionChapsClient::CreateObjectCallback callback) = 0;
  // Similar to PKCS #11 v2.20 section 11.7 page 131.
  virtual void DestroyObject(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      SessionChapsClient::DestroyObjectCallback callback) = 0;
  // Similar to DestroyObject, but handles multiple objects at once and retries
  // the deletion on failure.
  virtual void DestroyObjectsWithRetries(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      SessionChapsClient::DestroyObjectCallback callback) = 0;
  // Similar to PKCS #11 v2.20 section 11.7 page 133.
  // Tries to guess attribute sizes and when Chaps replies that the guessed size
  // is too small, queries the exact size and retries with it. If
  // CKR_ATTRIBUTE_SENSITIVE or CKR_ATTRIBUTE_TYPE_INVALID error is returned,
  // one or more attributes is not retrieved. If more than one attribute was
  // not retrieved, it's impossible to deduce whether the attribute is actually
  // there and available and just the guessed size was too small (especially
  // relevant for string attributes).
  virtual void GetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      std::vector<AttributeId> attribute_ids,
      HighLevelChapsClient::GetAttributeValueCallback callback) = 0;
  // Similar to PKCS #11 v2.20 section 11.7 page 135.
  virtual void SetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      const chaps::AttributeList& attributes,
      SessionChapsClient::SetAttributeValueCallback callback) = 0;
  // Same as SetAttributeValue above, but sets attributes on multiple objects at
  // once.
  virtual void SetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      const chaps::AttributeList& attributes,
      SessionChapsClient::SetAttributeValueCallback callback) = 0;
  // Combines FindObjects* methods, PKCS #11 v2.20 section 11.7 page 136-138.
  virtual void FindObjects(
      SessionChapsClient::SlotId slot_id,
      const chaps::AttributeList& attributes,
      SessionChapsClient::FindObjectsCallback callback) = 0;
  // Combines SignInit and Sign, PKCS #11 v2.20 section 11.7 page 152-153.
  // `mechanism_parameter` is the bytes of a struct containing the parameter.
  // RSA-PKCS1 and ECDSA mechanisms don't take any parameters, for RSA_PSS see
  // chromeos::PKCS11_CK_RSA_PKCS_PSS_PARAMS struct.
  virtual void Sign(SessionChapsClient::SlotId slot_id,
                    uint64_t mechanism_type,
                    const std::vector<uint8_t>& mechanism_parameter,
                    SessionChapsClient::ObjectHandle key_handle,
                    std::vector<uint8_t> data,
                    SessionChapsClient::SignCallback callback) = 0;
  // Similar to PKCS #11 v2.20 section 11.7 page 135.
  virtual void GenerateKeyPair(
      SessionChapsClient::SlotId slot_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const chaps::AttributeList& public_key_attributes,
      const chaps::AttributeList& private_key_attributes,
      SessionChapsClient::GenerateKeyPairCallback callback) = 0;
};

// Exported for unit tests and KcerFactory only.
class COMPONENT_EXPORT(KCER) HighLevelChapsClientImpl
    : public HighLevelChapsClient {
 public:
  explicit HighLevelChapsClientImpl(SessionChapsClient* session_chaps_client);
  ~HighLevelChapsClientImpl() override;

  // Implements HighLevelChapsClient.
  void GetMechanismList(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::GetMechanismListCallback callback) override;
  void CreateObject(SessionChapsClient::SlotId slot_id,
                    const chaps::AttributeList& attributes,
                    SessionChapsClient::CreateObjectCallback callback) override;
  void DestroyObject(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      SessionChapsClient::DestroyObjectCallback callback) override;
  void DestroyObjectsWithRetries(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      SessionChapsClient::DestroyObjectCallback callback) override;
  void GetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      std::vector<AttributeId> attribute_ids,
      HighLevelChapsClient::GetAttributeValueCallback callback) override;
  void SetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      const chaps::AttributeList& attributes,
      SessionChapsClient::SetAttributeValueCallback callback) override;
  void SetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      const chaps::AttributeList& attributes,
      SessionChapsClient::SetAttributeValueCallback callback) override;
  void FindObjects(SessionChapsClient::SlotId slot_id,
                   const chaps::AttributeList& attributes,
                   SessionChapsClient::FindObjectsCallback callback) override;
  void Sign(SessionChapsClient::SlotId slot_id,
            uint64_t mechanism_type,
            const std::vector<uint8_t>& mechanism_parameter,
            SessionChapsClient::ObjectHandle key_handle,
            std::vector<uint8_t> data,
            SessionChapsClient::SignCallback callback) override;
  void GenerateKeyPair(
      SessionChapsClient::SlotId slot_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const chaps::AttributeList& public_key_attributes,
      const chaps::AttributeList& private_key_attributes,
      SessionChapsClient::GenerateKeyPairCallback callback) override;

  void SetSessionChapsClientForTesting(
      SessionChapsClient* session_chaps_client);

 private:
  void DestroyObjectsWithRetriesImpl(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      std::vector<SessionChapsClient::ObjectHandle> failed_handles,
      uint32_t last_error,
      int retries_left,
      SessionChapsClient::DestroyObjectCallback callback);
  void DestroyObjectsWithRetriesHandleOneResult(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      std::vector<SessionChapsClient::ObjectHandle> failed_handles,
      uint32_t last_error,
      int retries_left,
      SessionChapsClient::DestroyObjectCallback callback,
      uint32_t result_code);
  void DidGetAttributeValue(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      HighLevelChapsClient::GetAttributeValueCallback callback,
      std::vector<uint8_t> attributes,
      uint32_t result_code);
  void DidGetAttributeLength(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      HighLevelChapsClient::GetAttributeValueCallback callback,
      std::vector<uint8_t> attributes,
      uint32_t result_code);
  void DidGetAttributeValueWithLength(
      SessionChapsClient::SlotId slot_id,
      SessionChapsClient::ObjectHandle object_handle,
      HighLevelChapsClient::GetAttributeValueCallback callback,
      std::vector<uint8_t> attributes,
      uint32_t result_code);

  void SetAttributeValueImpl(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> object_handles,
      const chaps::AttributeList& attributes,
      SessionChapsClient::SetAttributeValueCallback callback,
      uint32_t last_error,
      uint32_t new_result_code);

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<SessionChapsClient> session_chaps_client_;
  base::WeakPtrFactory<HighLevelChapsClientImpl> weak_factory_{this};
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_CHAPS_HIGH_LEVEL_CHAPS_CLIENT_H_

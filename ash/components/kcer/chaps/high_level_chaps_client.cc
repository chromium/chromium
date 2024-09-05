// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/chaps/high_level_chaps_client.h"

#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/key_permissions.pb.h"
#include "base/sequence_checker.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace kcer {

void AddAttribute(chaps::AttributeList& attr_list,
                  chromeos::PKCS11_CK_ATTRIBUTE_TYPE type,
                  base::span<const uint8_t> data) {
  chaps::Attribute* new_attr = attr_list.add_attributes();
  new_attr->set_type(type);
  new_attr->set_value(std::string(data.begin(), data.end()));
  new_attr->set_length(data.size());
}

namespace {

using AttributeId = HighLevelChapsClient::AttributeId;

constexpr int kDefaultAttempts = 5;

// Returns the expected size of an attribute with `attribute_id`. In case the
// actual attribute is bigger, the retrieval should still succeed, but will take
// two additional D-Bus calls.
int GetDefaultLength(AttributeId attribute_id) {
  switch (attribute_id) {
    case AttributeId::kModulus:
      // The size of the modulus for a 2048 RSA public key. All other supported
      // keys are expected to be smaller or equal to this.
      return 256;
    case AttributeId::kPublicExponent:
      return 3;
    case AttributeId::kEcPoint:
      return 67;
    case AttributeId::kPkcs11Id:
      // The size of a SHA-1 hash, a typical size for CKA_ID.
      return 20;
    case AttributeId::kLabel:
      // An arbitrary length, label is just a user readable string. In same
      // cases it contains a GUID (38 characters).
      return 40;
    case AttributeId::kKeyType:
      return sizeof(chromeos::PKCS11_CK_KEY_TYPE);
    case AttributeId::kValue:
      return 800;
    case AttributeId::kKeyInSoftware:
      return sizeof(chromeos::PKCS11_CK_BBOOL);
    case AttributeId::kKeyPermissions:
      return sizeof(chaps::KeyPermissions);
    case AttributeId::kCertProvisioningId:
      // An arbitrary length, the id is just a user readable string. In same
      // cases it contains a GUID (38 characters).
      return 40;
  }
}

}  // namespace

HighLevelChapsClientImpl::HighLevelChapsClientImpl(
    SessionChapsClient* session_chaps_client)
    : session_chaps_client_(session_chaps_client) {}
HighLevelChapsClientImpl::~HighLevelChapsClientImpl() = default;

//==============================================================================

void HighLevelChapsClientImpl::GetMechanismList(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::GetMechanismListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_chaps_client_->GetMechanismList(slot_id, std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::CreateObject(
    SessionChapsClient::SlotId slot_id,
    const chaps::AttributeList& attributes,
    SessionChapsClient::CreateObjectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_chaps_client_->CreateObject(
      slot_id, SessionChapsClient::SerializeToBytes(attributes),
      kDefaultAttempts, std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::DestroyObject(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::ObjectHandle object_handle,
    SessionChapsClient::DestroyObjectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_chaps_client_->DestroyObject(slot_id, object_handle, kDefaultAttempts,
                                       std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::DestroyObjectsWithRetries(
    SessionChapsClient::SlotId slot_id,
    std::vector<SessionChapsClient::ObjectHandle> object_handles,
    SessionChapsClient::DestroyObjectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyObjectsWithRetriesImpl(slot_id, std::move(object_handles),
                                /*failed_handles=*/{},
                                /*last_error=*/chromeos::PKCS11_CKR_OK,
                                /*retries_left=*/2, std::move(callback));
}

void HighLevelChapsClientImpl::DestroyObjectsWithRetriesImpl(
    SessionChapsClient::SlotId slot_id,
    std::vector<SessionChapsClient::ObjectHandle> object_handles,
    std::vector<SessionChapsClient::ObjectHandle> failed_handles,
    uint32_t last_error,
    int retries_left,
    SessionChapsClient::DestroyObjectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!object_handles.empty()) {
    SessionChapsClient::ObjectHandle next_handle = object_handles.back();
    auto chaps_callback = base::BindOnce(
        &HighLevelChapsClientImpl::DestroyObjectsWithRetriesHandleOneResult,
        weak_factory_.GetWeakPtr(), slot_id, object_handles, failed_handles,
        last_error, retries_left, std::move(callback));
    return DestroyObject(slot_id, next_handle, std::move(chaps_callback));
  }

  if (!failed_handles.empty() && (retries_left > 0)) {
    return DestroyObjectsWithRetriesImpl(
        slot_id, /*object_handles=*/std::move(failed_handles),
        /*failed_handles=*/{}, last_error, retries_left - 1,
        std::move(callback));
  }

  uint32_t final_result =
      failed_handles.empty() ? chromeos::PKCS11_CKR_OK : last_error;
  return std::move(callback).Run(final_result);
}

void HighLevelChapsClientImpl::DestroyObjectsWithRetriesHandleOneResult(
    SessionChapsClient::SlotId slot_id,
    std::vector<SessionChapsClient::ObjectHandle> object_handles,
    std::vector<SessionChapsClient::ObjectHandle> failed_handles,
    uint32_t last_error,
    int retries_left,
    SessionChapsClient::DestroyObjectCallback callback,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return std::move(callback).Run(result_code);
  }

  uint32_t new_last_error = last_error;
  if (result_code != chromeos::PKCS11_CKR_OK) {
    failed_handles.push_back(object_handles.back());
    new_last_error = result_code;
  }
  object_handles.pop_back();

  return DestroyObjectsWithRetriesImpl(
      slot_id, std::move(object_handles), std::move(failed_handles),
      new_last_error, retries_left, std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::GetAttributeValue(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::ObjectHandle object_handle,
    std::vector<AttributeId> attribute_ids,
    HighLevelChapsClient::GetAttributeValueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  chaps::AttributeList attributes_query;
  for (AttributeId attr_id : attribute_ids) {
    chaps::Attribute* attr = attributes_query.add_attributes();
    attr->set_type(static_cast<uint32_t>(attr_id));

    int length = GetDefaultLength(attr_id);
    attr->set_length(length);
    // The "value" represents the buffer from the PKCS#11 interface. The buffer
    // needs to be big enough to store the attribute value. While communicating
    // over D-Bus this actual buffer won't be used and the reply will be sent in
    // a separate message, but Chaps looks at the size of it to allocate the
    // memory for the reply.
    // TODO(miersh): Since Chaps only needs the size, it could just looks at the
    // "length" field. Investigate why it's not doing that and optimize the
    // protocol.
    attr->set_value(std::string(/*count=*/length, /*ch=*/'\0'));
  }

  auto chaps_callback = base::BindOnce(
      &HighLevelChapsClientImpl::DidGetAttributeValue,
      weak_factory_.GetWeakPtr(), slot_id, object_handle, std::move(callback));
  session_chaps_client_->GetAttributeValue(
      slot_id, object_handle,
      SessionChapsClient::SerializeToBytes(attributes_query), kDefaultAttempts,
      std::move(chaps_callback));
}

void HighLevelChapsClientImpl::DidGetAttributeValue(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::ObjectHandle object_handle,
    HighLevelChapsClient::GetAttributeValueCallback callback,
    std::vector<uint8_t> attributes,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  chaps::AttributeList decoded_attributes;
  if (!decoded_attributes.ParseFromArray(attributes.data(),
                                         attributes.size())) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_DECODING_ERROR);
  }

  if (result_code != chromeos::PKCS11_CKR_BUFFER_TOO_SMALL) {
    // If `result_code` is ok, then `decoded_attributes` should contain the
    // result. If `result_code` is an error, just forward it to the caller
    // together with all attributes that chaps managed to find (if any).
    return std::move(callback).Run(std::move(decoded_attributes), result_code);
  }

  // One or more provided lengths for the attributes were too small, Chaps
  // returned only some of the attributes. In theory only such attributes need
  // to be processed specially, but for simplicity this class requests correct
  // lengths for all attributes and then queries the attributes again.
  for (int i = 0; i < decoded_attributes.attributes_size(); i++) {
    // Clearing the value requests the size of the stored attribute.
    decoded_attributes.mutable_attributes(i)->clear_value();
    decoded_attributes.mutable_attributes(i)->set_length(0);
  }

  auto chaps_callback = base::BindOnce(
      &HighLevelChapsClientImpl::DidGetAttributeLength,
      weak_factory_.GetWeakPtr(), slot_id, object_handle, std::move(callback));
  session_chaps_client_->GetAttributeValue(
      slot_id, object_handle,
      SessionChapsClient::SerializeToBytes(decoded_attributes),
      kDefaultAttempts, std::move(chaps_callback));
}

void HighLevelChapsClientImpl::DidGetAttributeLength(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::ObjectHandle object_handle,
    HighLevelChapsClient::GetAttributeValueCallback callback,
    std::vector<uint8_t> attributes,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(callback).Run({}, result_code);
  }

  chaps::AttributeList decoded_attributes;
  if (!decoded_attributes.ParseFromArray(attributes.data(),
                                         attributes.size())) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_DECODING_ERROR);
  }
  // Set values to requests the attributes with the returned lengths.
  for (int i = 0; i < decoded_attributes.attributes_size(); i++) {
    decoded_attributes.mutable_attributes(i)->set_value(std::string(
        /*count=*/decoded_attributes.attributes(i).length(), /*ch=*/'\0'));
  }

  auto chaps_callback = base::BindOnce(
      &HighLevelChapsClientImpl::DidGetAttributeValueWithLength,
      weak_factory_.GetWeakPtr(), slot_id, object_handle, std::move(callback));
  session_chaps_client_->GetAttributeValue(
      slot_id, object_handle,
      SessionChapsClient::SerializeToBytes(decoded_attributes),
      kDefaultAttempts, std::move(chaps_callback));
}

void HighLevelChapsClientImpl::DidGetAttributeValueWithLength(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::ObjectHandle object_handle,
    HighLevelChapsClient::GetAttributeValueCallback callback,
    std::vector<uint8_t> attributes,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  chaps::AttributeList decoded_attributes;
  if (!decoded_attributes.ParseFromArray(attributes.data(),
                                         attributes.size())) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_DECODING_ERROR);
  }
  return std::move(callback).Run(std::move(decoded_attributes), result_code);
}

//==============================================================================

void HighLevelChapsClientImpl::SetAttributeValue(
    SessionChapsClient::SlotId slot_id,
    SessionChapsClient::ObjectHandle object_handle,
    const chaps::AttributeList& attributes,
    SessionChapsClient::SetAttributeValueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_chaps_client_->SetAttributeValue(
      slot_id, object_handle, SessionChapsClient::SerializeToBytes(attributes),
      kDefaultAttempts, std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::SetAttributeValue(
    SessionChapsClient::SlotId slot_id,
    std::vector<SessionChapsClient::ObjectHandle> object_handles,
    const chaps::AttributeList& attributes,
    SessionChapsClient::SetAttributeValueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAttributeValueImpl(slot_id, object_handles, std::move(attributes),
                        std::move(callback),
                        /*last_error=*/chromeos::PKCS11_CKR_OK,
                        /*new_result_code=*/chromeos::PKCS11_CKR_OK);
}

void HighLevelChapsClientImpl::SetAttributeValueImpl(
    SessionChapsClient::SlotId slot_id,
    std::vector<SessionChapsClient::ObjectHandle> object_handles,
    const chaps::AttributeList& attributes,
    SessionChapsClient::SetAttributeValueCallback callback,
    uint32_t last_error,
    uint32_t new_result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_result_code != chromeos::PKCS11_CKR_OK) {
    last_error = new_result_code;
  }

  if (object_handles.empty()) {
    return std::move(callback).Run(last_error);
  }

  SessionChapsClient::ObjectHandle next_handle = object_handles.back();
  chaps::AttributeList attributes_copy = attributes;
  object_handles.pop_back();
  auto chaps_callback =
      base::BindOnce(&HighLevelChapsClientImpl::SetAttributeValueImpl,
                     weak_factory_.GetWeakPtr(), slot_id, object_handles,
                     std::move(attributes), std::move(callback), last_error);
  SetAttributeValue(slot_id, next_handle, std::move(attributes_copy),
                    std::move(chaps_callback));
}

//==============================================================================

void HighLevelChapsClientImpl::FindObjects(
    SessionChapsClient::SlotId slot_id,
    const chaps::AttributeList& attributes,
    SessionChapsClient::FindObjectsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_chaps_client_->FindObjects(
      slot_id, SessionChapsClient::SerializeToBytes(attributes),
      kDefaultAttempts, std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::Sign(
    SessionChapsClient::SlotId slot_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    SessionChapsClient::ObjectHandle key_handle,
    std::vector<uint8_t> data,
    SessionChapsClient::SignCallback callback) {
  session_chaps_client_->Sign(slot_id, mechanism_type, mechanism_parameter,
                              key_handle, std::move(data), kDefaultAttempts,
                              std::move(callback));
}

//==============================================================================

void HighLevelChapsClientImpl::GenerateKeyPair(
    SessionChapsClient::SlotId slot_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const chaps::AttributeList& public_attributes,
    const chaps::AttributeList& private_attributes,
    SessionChapsClient::GenerateKeyPairCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_chaps_client_->GenerateKeyPair(
      slot_id, mechanism_type, mechanism_parameter,
      SessionChapsClient::SerializeToBytes(public_attributes),
      SessionChapsClient::SerializeToBytes(private_attributes),
      kDefaultAttempts, std::move(callback));
}

}  // namespace kcer

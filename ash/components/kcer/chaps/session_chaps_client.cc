// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/chaps/session_chaps_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "ash/components/kcer/attributes.pb.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace kcer {
namespace {

// Arbitrary limit for the FindObjects method, equals to 8Mb of object handles
// (each handle is 8 bytes), should be more than enough. The main
// consideration is the amount of data sent over D-Bus at once and the time
// required to process it in Chrome. The current implementation doesn't expect
// Chaps to hold too many objects (usually <100 of a given type, <10000 99.9%
// of the time), the processing of the objects is handled on the UI thread. If
// we find a need to process millions of objects, FindObjects can be changed
// to do multiple smaller requests and the objects could be processed on a
// worker thread.
inline constexpr uint64_t kFindObjectsMaxCount = 1 << 20;

// Pass CKF_RW_SESSION so Chrome can modify data, e.g. generate new key pairs.
// CKF_SERIAL_SESSION should always be set according to
// http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-os.html#_Toc416959688
// and chaps verifies that.
inline constexpr uint64_t kSessionFlags =
    chromeos::PKCS11_CKF_RW_SESSION | chromeos::PKCS11_CKF_SERIAL_SESSION;

}  // namespace

SessionChapsClient::SessionChapsClient() = default;
SessionChapsClient::~SessionChapsClient() = default;

SessionChapsClientImpl::SessionChapsClientImpl() = default;
SessionChapsClientImpl::~SessionChapsClientImpl() = default;

// static
std::vector<uint8_t> SessionChapsClient::SerializeToBytes(
    const chaps::AttributeList& attr_list) {
  std::vector<uint8_t> result;
  result.resize(attr_list.ByteSizeLong());
  attr_list.SerializeToArray(result.data(), result.size());
  return result;
}

// static
bool SessionChapsClient::IsSessionError(uint32_t result_code) {
  return result_code == chromeos::PKCS11_CKR_SESSION_HANDLE_INVALID ||
         result_code == chromeos::PKCS11_CKR_SESSION_CLOSED;
}

//==============================================================================

void SessionChapsClientImpl::GetMechanismList(
    SlotId slot_id,
    GetMechanismListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  return chaps_service->GetMechanismList(slot_id.value(), std::move(callback));
}

//==============================================================================

void SessionChapsClientImpl::CreateObject(
    SlotId slot_id,
    const std::vector<uint8_t>& attributes,
    int attempts_left,
    CreateObjectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run(ObjectHandle(0),
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run(ObjectHandle(0),
                                     chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::CreateObject, weak_factory_.GetWeakPtr(),
        slot_id, attributes, attempts_left, std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->CreateObject(
      session_id.value(), attributes,
      base::BindOnce(&SessionChapsClientImpl::DidCreateObject,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidCreateObject(SlotId slot_id,
                                             CreateObjectCallback callback,
                                             uint64_t object_handle,
                                             uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  return std::move(callback).Run(ObjectHandle(object_handle), result_code);
}

//==============================================================================

void SessionChapsClientImpl::DestroyObject(SlotId slot_id,
                                           ObjectHandle object_handle,
                                           int attempts_left,
                                           DestroyObjectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run(chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::DestroyObject, weak_factory_.GetWeakPtr(),
        slot_id, object_handle, attempts_left, std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->DestroyObject(
      session_id.value(), object_handle.value(),
      base::BindOnce(&SessionChapsClientImpl::DidDestroyObject,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidDestroyObject(SlotId slot_id,
                                              DestroyObjectCallback callback,
                                              uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  return std::move(callback).Run(result_code);
}

//==============================================================================

void SessionChapsClientImpl::GetAttributeValue(
    SlotId slot_id,
    ObjectHandle object_handle,
    std::vector<uint8_t> attributes_query,
    int attempts_left,
    GetAttributeValueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run({}, chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::GetAttributeValue, weak_factory_.GetWeakPtr(),
        slot_id, object_handle, std::move(attributes_query), attempts_left,
        std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->GetAttributeValue(
      session_id.value(), object_handle.value(), attributes_query,
      base::BindOnce(&SessionChapsClientImpl::DidGetAttributeValue,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidGetAttributeValue(
    SlotId slot_id,
    GetAttributeValueCallback callback,
    const std::vector<uint8_t>& attributes,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  return std::move(callback).Run(attributes, result_code);
}

//==============================================================================

void SessionChapsClientImpl::SetAttributeValue(
    SlotId slot_id,
    ObjectHandle object_handle,
    std::vector<uint8_t> attributes,
    int attempts_left,
    SetAttributeValueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run(chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::SetAttributeValue, weak_factory_.GetWeakPtr(),
        slot_id, object_handle, std::move(attributes), attempts_left,
        std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->SetAttributeValue(
      session_id.value(), object_handle.value(), attributes,
      base::BindOnce(&SessionChapsClientImpl::DidSetAttributeValue,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidSetAttributeValue(
    SlotId slot_id,
    SetAttributeValueCallback callback,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  return std::move(callback).Run(result_code);
}

//==============================================================================

void SessionChapsClientImpl::FindObjects(SlotId slot_id,
                                         std::vector<uint8_t> attributes,
                                         int attempts_left,
                                         FindObjectsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);

  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run({}, chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::FindObjects, weak_factory_.GetWeakPtr(),
        slot_id, std::move(attributes), attempts_left, std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->FindObjectsInit(
      session_id.value(), attributes,
      base::BindOnce(&SessionChapsClientImpl::DidFindObjectsInit,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidFindObjectsInit(SlotId slot_id,
                                                FindObjectsCallback callback,
                                                uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
    return std::move(callback).Run({}, result_code);
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(callback).Run({}, result_code);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  CHECK_NE(session_id.value(), chromeos::PKCS11_INVALID_SESSION_ID);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_service->FindObjects(
      session_id.value(), kFindObjectsMaxCount,
      base::BindOnce(&SessionChapsClientImpl::DidFindObjects,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidFindObjects(
    SlotId slot_id,
    FindObjectsCallback callback,
    const std::vector<uint64_t>& object_list,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
    return std::move(callback).Run({}, result_code);
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(callback).Run({}, result_code);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  CHECK_NE(session_id.value(), chromeos::PKCS11_INVALID_SESSION_ID);

  std::vector<ObjectHandle> typed_list(object_list.begin(), object_list.end());

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_service->FindObjectsFinal(
      session_id.value(),
      base::BindOnce(&SessionChapsClientImpl::DidFindObjectsFinal,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback),
                     std::move(typed_list)));
}

void SessionChapsClientImpl::DidFindObjectsFinal(
    SlotId slot_id,
    FindObjectsCallback callback,
    std::vector<ObjectHandle> object_list,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    // It's not clear if finishing a search can actually fail. The caller
    // shouldn't need to care since the objects were already successfully
    // retrieved, but future FindObjects calls within the same PKCS#11 session
    // will fail if the current search is not finished properly.
    LOG(ERROR) << "Failed to call FindObjectsFinal";
  }
  return std::move(callback).Run(std::move(object_list),
                                 chromeos::PKCS11_CKR_OK);
}

//==============================================================================

void SessionChapsClientImpl::Sign(SlotId slot_id,
                                  uint64_t mechanism_type,
                                  std::vector<uint8_t> mechanism_parameter,
                                  ObjectHandle key_handle,
                                  std::vector<uint8_t> data,
                                  int attempts_left,
                                  SignCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run({}, chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::Sign, weak_factory_.GetWeakPtr(), slot_id,
        mechanism_type, std::move(mechanism_parameter), key_handle,
        std::move(data), attempts_left, std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->SignInit(
      session_id.value(), mechanism_type, mechanism_parameter,
      key_handle.value(),
      base::BindOnce(&SessionChapsClientImpl::DidSignInit,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(data),
                     std::move(callback)));
}

void SessionChapsClientImpl::DidSignInit(SlotId slot_id,
                                         std::vector<uint8_t> data,
                                         SignCallback callback,
                                         uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
    return std::move(callback).Run({}, result_code);
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(callback).Run({}, result_code);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  CHECK_NE(session_id.value(), chromeos::PKCS11_INVALID_SESSION_ID);

  // Maximum supported RSA key is 4096 bits, its signature is 4096 bits, 512
  // bytes. The only supported EC key is P-256, its signature is 512 bits, 64
  // bytes.
  constexpr uint64_t kMaxOutLength = 512;

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  return chaps_service->Sign(
      session_id.value(), data, kMaxOutLength,
      base::BindOnce(&SessionChapsClientImpl::DidSign,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidSign(SlotId slot_id,
                                     SignCallback callback,
                                     uint64_t actual_out_length,
                                     const std::vector<uint8_t>& signature,
                                     uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  return std::move(callback).Run(std::move(signature), result_code);
}

//==============================================================================

void SessionChapsClientImpl::GenerateKeyPair(
    SlotId slot_id,
    uint64_t mechanism_type,
    std::vector<uint8_t> mechanism_parameter,
    std::vector<uint8_t> public_attributes,
    std::vector<uint8_t> private_attributes,
    int attempts_left,
    GenerateKeyPairCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::ChapsClient* chaps_service = ash::ChapsClient::Get();
  if (!chaps_service) {
    return std::move(callback).Run(ObjectHandle(0), ObjectHandle(0),
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }

  SessionId session_id = GetSessionForSlot(slot_id);
  if (session_id.value() == chromeos::PKCS11_INVALID_SESSION_ID) {
    if (attempts_left <= 0) {
      return std::move(callback).Run(ObjectHandle(0), ObjectHandle(0),
                                     chaps::CKR_FAILED_TO_OPEN_SESSION);
    }
    attempts_left--;

    auto chaps_callback = base::BindOnce(
        &SessionChapsClientImpl::GenerateKeyPair, weak_factory_.GetWeakPtr(),
        slot_id, mechanism_type, std::move(mechanism_parameter),
        std::move(public_attributes), std::move(private_attributes),
        attempts_left, std::move(callback));
    return chaps_service->OpenSession(
        slot_id.value(), kSessionFlags,
        base::BindOnce(&SessionChapsClientImpl::SaveSessionId,
                       weak_factory_.GetWeakPtr(), slot_id,
                       std::move(chaps_callback)));
  }

  return chaps_service->GenerateKeyPair(
      session_id.value(), mechanism_type, mechanism_parameter,
      public_attributes, private_attributes,
      base::BindOnce(&SessionChapsClientImpl::DidGenerateKeyPair,
                     weak_factory_.GetWeakPtr(), slot_id, std::move(callback)));
}

void SessionChapsClientImpl::DidGenerateKeyPair(
    SlotId slot_id,
    GenerateKeyPairCallback callback,
    uint64_t public_key_handle,
    uint64_t private_key_handle,
    uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionError(result_code)) {
    sessions_map_.erase(slot_id);
  }
  return std::move(callback).Run(ObjectHandle(public_key_handle),
                                 ObjectHandle(private_key_handle), result_code);
}
//==============================================================================

void SessionChapsClientImpl::SaveSessionId(SlotId slot_id,
                                           base::OnceClosure callback,
                                           uint64_t session_id,
                                           uint32_t result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result_code == chromeos::PKCS11_CKR_OK) {
    sessions_map_[slot_id] = SessionId(session_id);
  } else {
    LOG(ERROR) << "Failed to open "
                  "session, error: "
               << result_code;
  }
  return std::move(callback).Run();
}

SessionChapsClient::SessionId SessionChapsClientImpl::GetSessionForSlot(
    SlotId slot_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = sessions_map_.find(slot_id);
  if (iter == sessions_map_.end()) {
    return SessionId(chromeos::PKCS11_INVALID_SESSION_ID);
  }

  return iter->second;
}

}  // namespace kcer

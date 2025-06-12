// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/chaps_service_ash.h"

#include "chromeos/ash/components/dbus/chaps/chaps_client.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace crosapi {

ChapsServiceAsh::ChapsServiceAsh() = default;
ChapsServiceAsh::~ChapsServiceAsh() = default;

void ChapsServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ChapsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ChapsServiceAsh::GetSlotList(bool token_present,
                                  GetSlotListCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*slot_list=*/std::vector<uint64_t>(),
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->GetSlotList(token_present, std::move(callback));
}

void ChapsServiceAsh::GetMechanismList(uint64_t slot_id,
                                       GetMechanismListCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*slot_list=*/std::vector<uint64_t>(),
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->GetMechanismList(slot_id, std::move(callback));
}

void ChapsServiceAsh::OpenSession(uint64_t slot_id,
                                  uint64_t flags,
                                  OpenSessionCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(
        /*session_id=*/chromeos::PKCS11_INVALID_SESSION_ID,
        chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->OpenSession(slot_id, flags, std::move(callback));
}

void ChapsServiceAsh::CloseSession(uint64_t session_id,
                                   CloseSessionCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->CloseSession(session_id, std::move(callback));
}

void ChapsServiceAsh::CreateObject(uint64_t session_id,
                                   const std::vector<uint8_t>& attributes,
                                   CreateObjectCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*new_object_handle=*/0,
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->CreateObject(session_id, attributes,
                                         std::move(callback));
}

void ChapsServiceAsh::DestroyObject(uint64_t session_id,
                                    uint64_t object_handle,
                                    DestroyObjectCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->DestroyObject(session_id, object_handle,
                                          std::move(callback));
}

void ChapsServiceAsh::GetAttributeValue(
    uint64_t session_id,
    uint64_t object_handle,
    const std::vector<uint8_t>& attributes_in,
    GetAttributeValueCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*attributes_out=*/std::vector<uint8_t>(),
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->GetAttributeValue(
      session_id, object_handle, attributes_in, std::move(callback));
}

void ChapsServiceAsh::SetAttributeValue(uint64_t session_id,
                                        uint64_t object_handle,
                                        const std::vector<uint8_t>& attributes,
                                        SetAttributeValueCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->SetAttributeValue(session_id, object_handle,
                                              attributes, std::move(callback));
}

void ChapsServiceAsh::FindObjectsInit(uint64_t session_id,
                                      const std::vector<uint8_t>& attributes,
                                      FindObjectsInitCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->FindObjectsInit(session_id, attributes,
                                            std::move(callback));
}

void ChapsServiceAsh::FindObjects(uint64_t session_id,
                                  uint64_t max_object_count,
                                  FindObjectsCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*object_list=*/std::vector<uint64_t>(),
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->FindObjects(session_id, max_object_count,
                                        std::move(callback));
}

void ChapsServiceAsh::FindObjectsFinal(uint64_t session_id,
                                       FindObjectsFinalCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->FindObjectsFinal(session_id, std::move(callback));
}

void ChapsServiceAsh::EncryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    EncryptInitCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->EncryptInit(session_id, mechanism_type,
                                        mechanism_parameter, key_handle,
                                        std::move(callback));
}

void ChapsServiceAsh::Encrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              EncryptCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(0, {}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->Encrypt(session_id, data, max_out_length,
                                    std::move(callback));
}

void ChapsServiceAsh::DecryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    DecryptInitCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->DecryptInit(session_id, mechanism_type,
                                        mechanism_parameter, key_handle,
                                        std::move(callback));
}

void ChapsServiceAsh::Decrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DecryptCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(0, {}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->Decrypt(session_id, data, max_out_length,
                                    std::move(callback));
}

void ChapsServiceAsh::SignInit(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle,
                               SignInitCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->SignInit(session_id, mechanism_type,
                                     mechanism_parameter, key_handle,
                                     std::move(callback));
}

void ChapsServiceAsh::Sign(uint64_t session_id,
                           const std::vector<uint8_t>& data,
                           uint64_t max_out_length,
                           SignCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(0, {}, chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->Sign(session_id, data, max_out_length,
                                 std::move(callback));
}

void ChapsServiceAsh::GenerateKeyPair(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const std::vector<uint8_t>& public_attributes,
    const std::vector<uint8_t>& private_attributes,
    GenerateKeyPairCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*public_key_handle=*/0,
                                   /*private_key_handle=*/0,
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->GenerateKeyPair(
      session_id, mechanism_type, mechanism_parameter, public_attributes,
      private_attributes, std::move(callback));
}

void ChapsServiceAsh::WrapKey(uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t wrapping_key_handle,
                              uint64_t key_handle,
                              uint64_t max_out_length,
                              WrapKeyCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*actual_out_length=*/0,
                                   /*wrapped_key=*/{},
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->WrapKey(
      session_id, mechanism_type, mechanism_parameter, wrapping_key_handle,
      key_handle, max_out_length, std::move(callback));
}

void ChapsServiceAsh::UnwrapKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t wrapping_key_handle,
                                const std::vector<uint8_t>& wrapped_key,
                                const std::vector<uint8_t>& attributes,
                                UnwrapKeyCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*key_handle=*/0,
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->UnwrapKey(
      session_id, mechanism_type, mechanism_parameter, wrapping_key_handle,
      wrapped_key, attributes, std::move(callback));
}

void ChapsServiceAsh::DeriveKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t base_key_handle,
                                const std::vector<uint8_t>& attributes,
                                DeriveKeyCallback callback) {
  ash::ChapsClient* chaps_dbus_client = ash::ChapsClient::Get();
  if (!chaps_dbus_client) {
    return std::move(callback).Run(/*key_handle=*/0,
                                   chaps::CKR_DBUS_CLIENT_IS_NULL);
  }
  return chaps_dbus_client->DeriveKey(session_id, mechanism_type,
                                      mechanism_parameter, base_key_handle,
                                      attributes, std::move(callback));
}

}  // namespace crosapi

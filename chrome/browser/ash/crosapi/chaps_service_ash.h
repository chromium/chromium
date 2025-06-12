// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CHAPS_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CHAPS_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/chaps_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

class ChapsServiceAsh : public mojom::ChapsService {
 public:
  ChapsServiceAsh();
  ChapsServiceAsh(const ChapsServiceAsh&) = delete;
  ChapsServiceAsh& operator=(const ChapsServiceAsh&) = delete;
  ~ChapsServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ChapsService> receiver);

  // Implements mojom::ChapsService.
  void GetSlotList(bool token_present, GetSlotListCallback callback) override;
  void GetMechanismList(uint64_t slot_id,
                        GetMechanismListCallback callback) override;
  void OpenSession(uint64_t slot_id,
                   uint64_t flags,
                   OpenSessionCallback callback) override;
  void CloseSession(uint64_t session_id,
                    CloseSessionCallback callback) override;
  void CreateObject(uint64_t session_id,
                    const std::vector<uint8_t>& attributes,
                    CreateObjectCallback callback) override;
  void DestroyObject(uint64_t session_id,
                     uint64_t object_handle,
                     DestroyObjectCallback callback) override;
  void GetAttributeValue(uint64_t session_id,
                         uint64_t object_handle,
                         const std::vector<uint8_t>& attributes,
                         GetAttributeValueCallback callback) override;
  void SetAttributeValue(uint64_t session_id,
                         uint64_t object_handle,
                         const std::vector<uint8_t>& attributes,
                         SetAttributeValueCallback callback) override;
  void FindObjectsInit(uint64_t session_id,
                       const std::vector<uint8_t>& attributes,
                       FindObjectsInitCallback callback) override;
  void FindObjects(uint64_t session_id,
                   uint64_t max_object_count,
                   FindObjectsCallback callback) override;
  void FindObjectsFinal(uint64_t session_id,
                        FindObjectsFinalCallback callback) override;
  void EncryptInit(uint64_t session_id,
                   uint64_t mechanism_type,
                   const std::vector<uint8_t>& mechanism_parameter,
                   uint64_t key_handle,
                   EncryptInitCallback callback) override;
  void Encrypt(uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               EncryptCallback callback) override;
  void DecryptInit(uint64_t session_id,
                   uint64_t mechanism_type,
                   const std::vector<uint8_t>& mechanism_parameter,
                   uint64_t key_handle,
                   DecryptInitCallback callback) override;
  void Decrypt(uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DecryptCallback callback) override;
  void SignInit(uint64_t session_id,
                uint64_t mechanism_type,
                const std::vector<uint8_t>& mechanism_parameter,
                uint64_t key_handle,
                SignInitCallback callback) override;
  void Sign(uint64_t session_id,
            const std::vector<uint8_t>& data,
            uint64_t max_out_length,
            SignCallback callback) override;
  void GenerateKeyPair(uint64_t session_id,
                       uint64_t mechanism_type,
                       const std::vector<uint8_t>& mechanism_parameter,
                       const std::vector<uint8_t>& public_attributes,
                       const std::vector<uint8_t>& private_attributes,
                       GenerateKeyPairCallback callback) override;
  void WrapKey(uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t wrapping_key_handle,
               uint64_t key_handle,
               uint64_t max_out_length,
               WrapKeyCallback callback) override;
  void UnwrapKey(uint64_t session_id,
                 uint64_t mechanism_type,
                 const std::vector<uint8_t>& mechanism_parameter,
                 uint64_t wrapping_key_handle,
                 const std::vector<uint8_t>& wrapped_key,
                 const std::vector<uint8_t>& attributes,
                 UnwrapKeyCallback callback) override;
  void DeriveKey(uint64_t session_id,
                 uint64_t mechanism_type,
                 const std::vector<uint8_t>& mechanism_parameter,
                 uint64_t base_key_handle,
                 const std::vector<uint8_t>& attributes,
                 DeriveKeyCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::ChapsService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CHAPS_SERVICE_ASH_H_

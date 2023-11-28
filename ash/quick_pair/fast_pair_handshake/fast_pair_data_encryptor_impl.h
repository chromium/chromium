// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_key_pair.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"

namespace ash {
namespace quick_pair {

class DeviceMetadata;

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptorImpl : public FastPairDataEncryptor {
 public:
  class Factory {
   public:
    static void CreateAsync(
        scoped_refptr<Device> device,
        base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback);

    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();

    virtual void CreateInstance(
        scoped_refptr<Device> device,
        base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback) = 0;

   private:
    static void CreateAsyncWithKeyExchange(
        scoped_refptr<Device> device,
        base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback);

    static void CreateAsyncWithAccountKey(
        scoped_refptr<Device> device,
        base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback);

    static void DeviceMetadataRetrieved(
        scoped_refptr<Device> device,
        base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback,
        DeviceMetadata* device_metadata,
        bool has_retryable_error);
  };

  // FastPairDataEncryptor
  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) override;
  const std::optional<std::array<uint8_t, kPublicKeyByteSize>>& GetPublicKey()
      override;
  void ParseDecryptedResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      base::OnceCallback<void(const std::optional<DecryptedResponse>&)>
          callback) override;
  void ParseDecryptedPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      base::OnceCallback<void(const std::optional<DecryptedPasskey>&)> callback)
      override;
  std::vector<uint8_t> CreateAdditionalDataPacket(
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& additional_data) override;
  bool VerifyEncryptedAdditionalData(
      const std::array<uint8_t, kHmacVerifyLenBytes> hmacSha256First8Bytes,
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& encrypted_additional_data) override;
  std::vector<uint8_t> EncryptAdditionalDataWithSecretKey(
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& additional_data) override;

  ~FastPairDataEncryptorImpl() override;

 protected:
  explicit FastPairDataEncryptorImpl(
      const fast_pair_encryption::KeyPair& key_pair);
  explicit FastPairDataEncryptorImpl(
      const std::array<uint8_t, kPrivateKeyByteSize>& secret_key);
  FastPairDataEncryptorImpl(const FastPairDataEncryptorImpl&) = delete;
  FastPairDataEncryptorImpl& operator=(const FastPairDataEncryptorImpl&) =
      delete;

 private:
  void QuickPairProcessStoppedOnResponse(
      QuickPairProcessManager::ShutdownReason shutdown_reason);
  void QuickPairProcessStoppedOnPasskey(
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  std::array<uint8_t, kPrivateKeyByteSize> secret_key_;

  // The public key is only required during initial pairing and optional during
  // communication with paired devices.
  std::optional<std::array<uint8_t, kPublicKeyByteSize>> public_key_ =
      std::nullopt;

  base::WeakPtrFactory<FastPairDataEncryptorImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_

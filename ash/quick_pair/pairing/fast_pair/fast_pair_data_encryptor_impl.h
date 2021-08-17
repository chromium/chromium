// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_key_pair.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

struct DeviceMetadata;

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
    static void DeviceMetadataRetrieved(
        scoped_refptr<Device> device,
        base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback,
        DeviceMetadata* device_metadata);
  };

  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) override;

  ~FastPairDataEncryptorImpl() override;

 protected:
  FastPairDataEncryptorImpl(const fast_pair_encryption::KeyPair& key_pair);
  FastPairDataEncryptorImpl(const FastPairDataEncryptorImpl&) = delete;
  FastPairDataEncryptorImpl& operator=(const FastPairDataEncryptorImpl&) =
      delete;

 private:
  std::array<uint8_t, kPrivateKeyByteSize> secret_key_;

  // The public key is only required during initial pairing and optional during
  // communication with paired devices.
  absl::optional<std::array<uint8_t, kPublicKeyByteSize>> public_key_ =
      absl::nullopt;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_

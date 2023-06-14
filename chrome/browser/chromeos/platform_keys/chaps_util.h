// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_CHAPS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_CHAPS_UTIL_H_

#include <pk11pub.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "crypto/scoped_nss_types.h"

namespace chromeos {
namespace platform_keys {

// Utility to perform operations on the chapsd daemon in a way that is
// compatible with NSS.
class ChapsUtil {
 public:
  // Creates a ChapsUtil instance.
  static std::unique_ptr<ChapsUtil> Create();

  virtual ~ChapsUtil() = default;

  // Generates a new software-backed RSA key pair of size |num_bits| in |slot|.
  // Returns true on success and false on failure. The generate key will have a
  // CKA_ID configured on both the public and private key objects which allows
  // NSS to work with it.
  // This is an expensive, blocking operation and may only be performed on a
  // worker thread.
  virtual bool GenerateSoftwareBackedRSAKey(
      PK11SlotInfo* slot,
      uint16_t num_bits,
      crypto::ScopedSECKEYPublicKey* out_public_key,
      crypto::ScopedSECKEYPrivateKey* out_private_key) = 0;

  // Import key and all included certificates from PKCS12 container.
  // Imported objects will be stored in Chaps.
  // If some of certificates can not be imported they will be skipped and
  // Pkcs12ReaderStatusCode::kFailureDuringCertImport error will be logged.
  // `is_software_backed` specifies whether a hardware-backed or software-backed
  // storage is used.
  virtual bool ImportPkcs12Certificate(PK11SlotInfo* slot,
                                       const std::vector<uint8_t>& pkcs12_data,
                                       const std::string& password,
                                       bool is_software_backed) = 0;

  using FactoryCallback = base::RepeatingCallback<std::unique_ptr<ChapsUtil>()>;

  // Sets the factory which ChapsUtil::Create() will use to create ChapsUtil
  // instances.
  // The caller is responsible for resetting the factory by passing a null
  // callback.
  static void SetFactoryForTesting(const FactoryCallback& factory);
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_CHAPS_UTIL_H_

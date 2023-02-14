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

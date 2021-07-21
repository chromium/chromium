// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_ENCRYPTION_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_ENCRYPTION_H_

#include <string>

#include "ash/quick_pair/pairing/fast_pair/fast_pair_key_pair.h"
#include "base/component_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

COMPONENT_EXPORT(QUICK_PAIR_PAIRING)
KeyPair GenerateKeysWithEcdhKeyAgreement(
    const std::string& decoded_public_anti_spoofing);

COMPONENT_EXPORT(QUICK_PAIR_PAIRING)
void SetKeysForEcdhKeyAgreement(bssl::UniquePtr<EC_GROUP> ec_group,
                                bssl::UniquePtr<EC_KEY> ec_key);

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_ENCRYPTION_H_

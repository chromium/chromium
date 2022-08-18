// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/public/auth_factors_data.h"

#include <algorithm>

#include "ash/components/login/auth/public/cryptohome_key_constants.h"
#include "base/check_op.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"

namespace ash {

AuthFactorsData::AuthFactorsData(std::vector<cryptohome::KeyDefinition> keys)
    : keys_(std::move(keys)) {
  // Sort the keys by label, so that in case of ties (e.g., when choosing among
  // multiple legacy keys in `FindOnlinePasswordKey()`) we're not affected by
  // random factors that affect the input ordering of `keys`.
  std::sort(keys_.begin(), keys_.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.label.value() < rhs.label.value();
  });
}

AuthFactorsData::AuthFactorsData() = default;
AuthFactorsData::AuthFactorsData(const AuthFactorsData&) = default;
AuthFactorsData::AuthFactorsData(AuthFactorsData&&) = default;
AuthFactorsData::~AuthFactorsData() = default;
AuthFactorsData& AuthFactorsData::operator=(const AuthFactorsData&) = default;

const cryptohome::KeyDefinition* AuthFactorsData::FindOnlinePasswordKey()
    const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.label.value() == kCryptohomeGaiaKeyLabel)
      return &key_def;
  }
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    // Check if label starts with prefix and has required type.
    if ((key_def.label.value().find(kCryptohomeGaiaKeyLegacyLabelPrefix) ==
         0) &&
        key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD)
      return &key_def;
  }
  return nullptr;
}

const cryptohome::KeyDefinition* AuthFactorsData::FindKioskKey() const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PUBLIC_MOUNT)
      return &key_def;
  }
  return nullptr;
}

bool AuthFactorsData::HasPasswordKey(const std::string& label) const {
  DCHECK_NE(label, kCryptohomePinLabel);

  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD &&
        key_def.label.value() == label)
      return true;
  }
  return false;
}

const cryptohome::KeyDefinition* AuthFactorsData::FindPinKey() const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD &&
        key_def.policy.low_entropy_credential) {
      DCHECK_EQ(key_def.label.value(), kCryptohomePinLabel);
      return &key_def;
    }
  }
  return nullptr;
}

}  // namespace ash

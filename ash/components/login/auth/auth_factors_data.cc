// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_factors_data.h"

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/components/login/auth/cryptohome_key_constants.h"

namespace ash {

AuthFactorsData::AuthFactorsData(std::vector<cryptohome::KeyDefinition> keys)
    : keys_(std::move(keys)) {}

AuthFactorsData::AuthFactorsData() = default;
AuthFactorsData::AuthFactorsData(const AuthFactorsData&) = default;
AuthFactorsData::AuthFactorsData(AuthFactorsData&&) = default;
AuthFactorsData::~AuthFactorsData() = default;
AuthFactorsData& AuthFactorsData::operator=(const AuthFactorsData&) = default;

const cryptohome::KeyDefinition* AuthFactorsData::FindOnlinePasswordKey()
    const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.label == kCryptohomeGaiaKeyLabel)
      return &key_def;
  }
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    // Check if label starts with prefix and has required type.
    if ((key_def.label.find(kCryptohomeGaiaKeyLegacyLabelPrefix) == 0) &&
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

}  // namespace ash

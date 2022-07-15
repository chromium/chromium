// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_DATA_H_
#define ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_DATA_H_

#include <string>

#include "ash/components/cryptohome/cryptohome_parameters.h"

namespace ash {

// Public information about authentication keys configured for particular user.
// This class partially encapsulates implementation details of key definition
// (cryptohome::KeyData vs cryptohome::AuthFactor).
// Note that this information does not contain any key secrets.
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) AuthFactorsData {
 public:
  explicit AuthFactorsData(std::vector<cryptohome::KeyDefinition> keys);

  // Empty constructor is needed so that UserContext can be created.
  AuthFactorsData();
  // Copy constructor (and operator) are needed because UserContext is copyable.
  AuthFactorsData(const AuthFactorsData&);
  AuthFactorsData(AuthFactorsData&&);

  ~AuthFactorsData();

  AuthFactorsData& operator=(const AuthFactorsData&);

  // Returns metadata for the Password key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindOnlinePasswordKey() const;

  // Returns metadata for the Kiosk key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindKioskKey() const;

  // Checks if password key with given label exists.
  bool HasPasswordKey(const std::string& label) const;

  // Returns metadata for the PIN key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindPinKey() const;

 private:
  std::vector<cryptohome::KeyDefinition> keys_;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_DATA_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_CRYPTOHOME_KEY_CONSTANTS_H_
#define ASH_COMPONENTS_LOGIN_AUTH_CRYPTOHOME_KEY_CONSTANTS_H_

#include "base/component_export.h"

namespace ash {

COMPONENT_EXPORT(ASH_LOGIN_AUTH)
extern const char kCryptohomeGaiaKeyLabel[];

COMPONENT_EXPORT(ASH_LOGIN_AUTH)
extern const char kCryptohomeGaiaKeyLegacyLabelPrefix[];

COMPONENT_EXPORT(ASH_LOGIN_AUTH)
extern const char kCryptohomePinLabel[];

COMPONENT_EXPORT(ASH_LOGIN_AUTH)
extern const char kCryptohomePublicMountLabel[];

COMPONENT_EXPORT(ASH_LOGIN_AUTH)
extern const char kCryptohomeWildcardLabel[];

COMPONENT_EXPORT(ASH_LOGIN_AUTH)
extern const char kCryptohomeRecoveryKeyLabel[];

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::kCryptohomeGaiaKeyLabel;
}  // namespace chromeos

#endif  // ASH_COMPONENTS_LOGIN_AUTH_CRYPTOHOME_KEY_CONSTANTS_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_
#define ASH_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_

#include <string>

#include "ash/components/security_token_pin/constants.h"
#include "base/component_export.h"

namespace chromeos {
namespace security_token_pin {

// Generate an error message for a security pin token dialog, based on dialog
// parameters |error_label|, |attempts_left|, and |accept_input|.
COMPONENT_EXPORT(SECURITY_TOKEN_PIN)
std::u16string GenerateErrorMessage(ErrorLabel error_label,
                                    int attempts_left,
                                    bool accept_input);

}  // namespace security_token_pin
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when //c/comp/security_token_pin
// is moved th ash
namespace ash {
namespace security_token_pin {
using ::chromeos::security_token_pin::GenerateErrorMessage;
}  // namespace security_token_pin
}  // namespace ash

#endif  // ASH_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_

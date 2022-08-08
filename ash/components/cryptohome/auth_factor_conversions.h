// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_CONVERSIONS_H_
#define ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_CONVERSIONS_H_

#include "ash/components/cryptohome/auth_factor.h"
#include "ash/components/cryptohome/auth_factor_input.h"
#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"

namespace cryptohome {

COMPONENT_EXPORT(ASH_COMPONENTS_CRYPTOHOME)
AuthFactorType ConvertFactorTypeFromProto(user_data_auth::AuthFactorType type);
COMPONENT_EXPORT(ASH_COMPONENTS_CRYPTOHOME)
void SerializeAuthFactor(const AuthFactor& factor,
                         user_data_auth::AuthFactor* out_proto);
COMPONENT_EXPORT(ASH_COMPONENTS_CRYPTOHOME)
void SerializeAuthInput(const AuthFactorRef& ref,
                        const AuthFactorInput& auth_input,
                        user_data_auth::AuthInput* out_proto);
COMPONENT_EXPORT(ASH_COMPONENTS_CRYPTOHOME)
AuthFactor DeserializeAuthFactor(const user_data_auth::AuthFactor& proto,
                                 AuthFactorType fallback_type);

}  // namespace cryptohome

#endif  // ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_CONVERSIONS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FLEX_ENROLLMENT_TOKEN_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FLEX_ENROLLMENT_TOKEN_PROVIDER_H_

#include <optional>
#include <string>

namespace ash {
class OobeConfiguration;
}  // namespace ash

namespace policy {

// Returns the Flex enrollment token if the token is present in the OOBE config
// and the device is a legitimate candidate for attempting Flex Auto Enrollment.
// Returns an empty optional otherwise.
std::optional<std::string> GetFlexEnrollmentToken(
    ash::OobeConfiguration* oobe_config);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FLEX_ENROLLMENT_TOKEN_PROVIDER_H_

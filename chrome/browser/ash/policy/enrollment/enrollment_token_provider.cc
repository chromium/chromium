// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_token_provider.h"

#include "ash/constants/ash_switches.h"
#include "base/logging.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/oobe_configuration.h"

namespace policy {

std::optional<std::string> GetEnrollmentToken(
    const ash::OobeConfiguration* oobe_config) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!ash::switches::IsRevenBranding()) {
    return std::nullopt;
  }

  if (!oobe_config) {
    LOG(ERROR) << "OobeConfiguration is not initialized";
    return std::nullopt;
  }

  const std::string* enrollment_token =
      oobe_config->configuration().FindString(
          ash::configuration::kEnrollmentToken);
  if (enrollment_token && !enrollment_token->empty()) {
    return *enrollment_token;
  }
#endif
  return std::nullopt;
}

}  // namespace policy

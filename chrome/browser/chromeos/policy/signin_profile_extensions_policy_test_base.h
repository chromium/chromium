// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SIGNIN_PROFILE_EXTENSIONS_POLICY_TEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SIGNIN_PROFILE_EXTENSIONS_POLICY_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "components/version_info/channel.h"
#include "extensions/common/features/feature_channel.h"
namespace policy {

// Base class for testing sign-in profile apps/extensions that are installed via
// the device policy.
class SigninProfileExtensionsPolicyTestBase
    : public DevicePolicyCrosBrowserTest {
 protected:
  explicit SigninProfileExtensionsPolicyTestBase(version_info::Channel channel);

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetUpOnMainThread() override;

  void AddExtensionForForceInstallation(
      const std::string& extension_id,
      const std::string& update_manifest_relative_path);

  // Returns the initial profile, which is the original profile of the sign-in
  // profile. The apps/extensions specified in the policy will be installed into
  // the initial profile, and then become available in both.
  Profile* GetInitialProfile();

  const version_info::Channel channel_;

 private:
  const extensions::ScopedCurrentChannel scoped_current_channel_;

  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyTestBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SIGNIN_PROFILE_EXTENSIONS_POLICY_TEST_BASE_H_

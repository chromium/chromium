// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/login/signin_profile_extensions_policy_test_base.h"

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

SigninProfileExtensionsPolicyTestBase::SigninProfileExtensionsPolicyTestBase(
    version_info::Channel channel)
    : channel_(channel), scoped_current_channel_(channel) {}

void SigninProfileExtensionsPolicyTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kLoginManager);
  command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
}

void SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread() {
  // Mixins might configure and start test server
  extensions::policy_test_utils::SetUpEmbeddedTestServer(
      embedded_test_server());

  DevicePolicyCrosBrowserTest::SetUpOnMainThread();

  if (!embedded_test_server()->Started()) {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
}

void SigninProfileExtensionsPolicyTestBase::AddExtensionForForceInstallation(
    const std::string& extension_id,
    const std::string& update_manifest_relative_path) {
  const GURL update_manifest_url =
      embedded_test_server()->GetURL(update_manifest_relative_path);
  const std::string policy_item_value = base::ReplaceStringPlaceholders(
      "$1;$2", {extension_id, update_manifest_url.spec()}, nullptr);
  device_policy()
      ->payload()
      .mutable_device_login_screen_extensions()
      ->add_device_login_screen_extensions(policy_item_value);
  RefreshDevicePolicy();
}

Profile* SigninProfileExtensionsPolicyTestBase::GetInitialProfile() {
  auto* browser_context =
      ash::BrowserContextHelper::Get()->GetSigninBrowserContext();
  CHECK(browser_context);
  return Profile::FromBrowserContext(browser_context)->GetOriginalProfile();
}

}  // namespace policy

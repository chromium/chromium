// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FORCE_INSTALLED_AFFILIATED_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_API_FORCE_INSTALLED_AFFILIATED_EXTENSION_APITEST_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

class Browser;

namespace base {
class CommandLine;
}  // namespace base

namespace extensions {

class Extension;

// TODO(https://crbug.com/1082195) Create user affiliation test mixin to use in
// this class.

// TODO(https://crbug.com/1129486) This class is duplicating mixin functionality
// from MixinBasedInProcessBrowserTest. Move this into its own class and inherit
// from it instead.

// Helper class to test force-installed extensions in a
// affiliated/non-affiliated user profile.
class ForceInstalledAffiliatedExtensionApiTest : public ExtensionApiTest {
 public:
  explicit ForceInstalledAffiliatedExtensionApiTest(bool is_affiliated);
  ~ForceInstalledAffiliatedExtensionApiTest() override;

 protected:
  // ExtensionApiTest
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;
  void TearDown() override;

  const extensions::Extension* ForceInstallExtension(
      const std::string& extension_path,
      const std::string& pem_path);

  // Sets |custom_arg_value|, loads |page_url| and waits for an extension API
  // test pass/fail notification.
  void TestExtension(Browser* browser,
                     const GURL& page_url,
                     const base::Value& custom_arg_value);

  // Whether the user should be affiliated (= user and device affiliation IDs
  // overlap).
  const bool is_affiliated_;

  const AccountId affiliated_account_id_;
  policy::MockConfigurationPolicyProvider policy_provider_;
  chromeos::ScopedStubInstallAttributes test_install_attributes_;
  policy::DevicePolicyCrosTestHelper test_helper_;
  InProcessBrowserTestMixinHost mixin_host_;
  ExtensionForceInstallMixin force_install_mixin_{&mixin_host_};
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FORCE_INSTALLED_AFFILIATED_EXTENSION_APITEST_H_

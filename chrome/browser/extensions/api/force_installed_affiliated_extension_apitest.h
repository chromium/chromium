// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FORCE_INSTALLED_AFFILIATED_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_API_FORCE_INSTALLED_AFFILIATED_EXTENSION_APITEST_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/extensions/extension_apitest.h"
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

// TODO(https://crbug.com/1082195) Create force-installed extension and user
// affiliation test mixins to replace this class.

// Helper class to test force-installed extensions in a
// affiliated/non-affiliated user profile.
class ForceInstalledAffiliatedExtensionApiTest : public ExtensionApiTest {
 public:
  explicit ForceInstalledAffiliatedExtensionApiTest(bool is_affiliated);
  ~ForceInstalledAffiliatedExtensionApiTest() override;

 protected:
  // ExtensionApiTest
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  const extensions::Extension* ForceInstallExtension(
      const extensions::ExtensionId& extension_id,
      const std::string& update_manifest_path);

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
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FORCE_INSTALLED_AFFILIATED_EXTENSION_APITEST_H_

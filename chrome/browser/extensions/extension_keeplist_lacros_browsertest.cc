// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "content/public/test/browser_test.h"

namespace extensions {

class LacrosExtensionKeeplistTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ash_keeplist_browser_init_params_supported_ =
        !chromeos::BrowserParamsProxy::Get()->ExtensionKeepList().is_null();
  }

 protected:
  bool AshKeeplistFromBrowserInitParamsSupported() {
    return ash_keeplist_browser_init_params_supported_;
  }

 private:
  bool ash_keeplist_browser_init_params_supported_ = false;
};

// Test the Ash extension keeplist data in Lacros against Ash versions that
// support passing Ash extension keep list to Lacros with
// crosapi::mojom::BrowserInitParams.
IN_PROC_BROWSER_TEST_F(LacrosExtensionKeeplistTest,
                       AshKeeplistFromBrowserInitParamsSupported) {
  // This test does not apply to unsupported ash version.
  if (!AshKeeplistFromBrowserInitParamsSupported())
    GTEST_SKIP();

  // For Ash running in the version that supports passing Ash extension keep
  // list to Lacros with crosapi::mojom::BrowserInitParams, just do some minimum
  // sanity check to make sure the extension list passed from Ash is not empty.
  // We have a more sophiscaited test in extension_keeplist_ash_browsertest.cc
  // to verify the keep lists are idnetical in Ash and Lacros for such case.
  EXPECT_FALSE(extensions::GetExtensionsRunInOSAndStandaloneBrowser().empty());
  EXPECT_FALSE(
      extensions::GetExtensionAppsRunInOSAndStandaloneBrowser().empty());
  EXPECT_FALSE(extensions::GetExtensionsRunInOSOnly().empty());
  EXPECT_FALSE(extensions::GetExtensionAppsRunInOSOnly().empty());
}

// Test the Ash extension keeplist data in Lacros against older Ash versions
// that do NOT support passing Ash extension keep list to Lacros with
// crosapi::mojom::BrowserInitParams.
IN_PROC_BROWSER_TEST_F(LacrosExtensionKeeplistTest,
                       AshKeeplistFromBrowserInitParamsNotSupported) {
  // This test only applies to older ash version which does not support
  // passing Ash extension keeplist data via crosapi::mojom::BrowserInitParams.
  if (AshKeeplistFromBrowserInitParamsSupported())
    GTEST_SKIP();

  // Verify that Lacros uses the static compiled ash extension keep list.
  // This tests the backward compatibility support of ash extension keeplist.
  ASSERT_EQ(extensions::GetExtensionsRunInOSAndStandaloneBrowser().size(),
            ExtensionsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_EQ(extensions::GetExtensionAppsRunInOSAndStandaloneBrowser().size(),
            ExtensionAppsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_EQ(extensions::GetExtensionsRunInOSOnly().size(),
            ExtensionsRunInOSOnlyAllowlistSizeForTest());
  ASSERT_EQ(extensions::GetExtensionAppsRunInOSOnly().size(),
            ExtensionAppsRunInOSOnlyAllowlistSizeForTest());
}

}  // namespace extensions

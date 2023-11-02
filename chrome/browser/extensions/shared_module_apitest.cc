// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

// NB: We use LoadExtension instead of InstallExtension for shared modules so
// the public-keys in their manifests are used to generate the extension ID, so
// it can be imported correctly.  We use InstallExtension otherwise so the loads
// happen through the CRX installer which validates imports.

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedModule) {
  // import_pass depends on this shared module.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("shared")));

  EXPECT_TRUE(RunExtensionTest("shared_module/import_pass"));

  EXPECT_FALSE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module")
          .AppendASCII("import_wrong_version"), 0));
  EXPECT_FALSE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module")
          .AppendASCII("import_non_existent"), 0));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedModuleAllowlist) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("shared_module")
                                .AppendASCII("shared_allowlist")));

  EXPECT_FALSE(InstallExtension(test_data_dir_.AppendASCII("shared_module")
                                    .AppendASCII("import_not_in_allowlist"),
                                0));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedModuleInstallEvent) {
  ExtensionTestMessageListener listener1("ready");

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("shared"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("import_pass"),
      1));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedModuleLocale) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("shared"));
  ASSERT_TRUE(extension);

  EXPECT_TRUE(RunExtensionTest("shared_module/import_locales"));
}

}  // namespace extensions

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {
constexpr char kGoodExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
constexpr char kSimpleWithKeyExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";
}  // namespace

class ExtensionsDisabledBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionsDisabledBrowserTest() = default;

  ExtensionsDisabledBrowserTest(const ExtensionsDisabledBrowserTest&) = delete;
  ExtensionsDisabledBrowserTest& operator=(
      const ExtensionsDisabledBrowserTest&) = delete;

  ~ExtensionsDisabledBrowserTest() override = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // A little tricky: we disable extensions (via the commandline) on the
    // non-PRE run. The PRE run is responsible for installing the external
    // extension.
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    const char* test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (!base::StartsWith(test_name, "PRE_", base::CompareCase::SENSITIVE)) {
      command_line->AppendSwitch(::switches::kDisableExtensions);
    }
  }
};

// Tests installing a number of extensions, and then restarting Chrome with the
// --disable-extensions switch. Regression test for https://crbug.com/836624.
IN_PROC_BROWSER_TEST_F(ExtensionsDisabledBrowserTest,
                       PRE_TestStartupWithInstalledExtensions) {
  const Extension* unpacked_extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_key"));
  ASSERT_TRUE(unpacked_extension);
  EXPECT_EQ(mojom::ManifestLocation::kUnpacked, unpacked_extension->location());

  const Extension* internal_extension =
      LoadExtension(test_data_dir_.AppendASCII("good.crx"));
  ASSERT_TRUE(internal_extension);
  EXPECT_EQ(mojom::ManifestLocation::kInternal, internal_extension->location());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodExtensionId));
  EXPECT_TRUE(
      registry->enabled_extensions().GetByID(kSimpleWithKeyExtensionId));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(kGoodExtensionId));
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(kSimpleWithKeyExtensionId));
}
IN_PROC_BROWSER_TEST_F(ExtensionsDisabledBrowserTest,
                       TestStartupWithInstalledExtensions) {
  EXPECT_TRUE(ExtensionsBrowserClient::Get()->AreExtensionsDisabled(
      *base::CommandLine::ForCurrentProcess(), profile()));

  // Neither of the installed extensions should have been loaded or added to
  // the registry.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_FALSE(registry->GetInstalledExtension(kGoodExtensionId));
  EXPECT_FALSE(registry->GetInstalledExtension(kSimpleWithKeyExtensionId));

  // However, they should still be stored in the preferences.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(kGoodExtensionId));
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(kSimpleWithKeyExtensionId));
}

}  // namespace extensions

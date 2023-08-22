// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_extension_apitest.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "content/public/test/browser_test.h"

using crosapi::AshRequiresLacrosExtensionApiTest;

namespace {

bool IsIdenticalList(base::span<const base::StringPiece> keep_list_from_ash,
                     const std::vector<std::string>& keep_list_from_lacros) {
  if (keep_list_from_ash.size() != keep_list_from_lacros.size())
    return false;

  for (size_t i = 0; i < keep_list_from_ash.size(); ++i) {
    if (keep_list_from_ash[i] != keep_list_from_lacros[i])
      return false;
  }

  return true;
}

}  // namespace

namespace extensions {

// Ash extension keeplist data is controlled by Ash and passed to Lacros
// via crosapi::mojom::BrowserInitParams. This class helps to test the
// Ash extension keeplist data is always identical in Ash and Lacros.
class ExtensionKeeplistTest : public AshRequiresLacrosExtensionApiTest {
 protected:
  test::AshBrowserTestStarter ash_starter_;
};

IN_PROC_BROWSER_TEST_F(ExtensionKeeplistTest,
                       IdenticalAshKeeplistFromAshAndLacros) {
  if (!ash_starter_.HasLacrosArgument())
    return;

  // Get the Ash extension keeplist data from Lacros.
  base::test::TestFuture<crosapi::mojom::ExtensionKeepListPtr> future;
  GetStandaloneBrowserTestController()->GetExtensionKeeplist(
      future.GetCallback());
  auto mojo_keeplist = future.Take();

  // Verify the ash extension keeplist data from Ash and Lacros are identical.

  ASSERT_EQ(extensions::GetExtensionsRunInOSAndStandaloneBrowser().size(),
            ExtensionsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_TRUE(IsIdenticalList(
      extensions::GetExtensionsRunInOSAndStandaloneBrowser(),
      mojo_keeplist->extensions_run_in_os_and_standalonebrowser));

  ASSERT_EQ(extensions::GetExtensionAppsRunInOSAndStandaloneBrowser().size(),
            ExtensionAppsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_TRUE(IsIdenticalList(
      extensions::GetExtensionAppsRunInOSAndStandaloneBrowser(),
      mojo_keeplist->extension_apps_run_in_os_and_standalonebrowser));

  ASSERT_EQ(extensions::GetExtensionsRunInOSOnly().size(),
            ExtensionsRunInOSOnlyAllowlistSizeForTest());
  ASSERT_TRUE(IsIdenticalList(extensions::GetExtensionsRunInOSOnly(),
                              mojo_keeplist->extensions_run_in_os_only));

  ASSERT_EQ(extensions::GetExtensionAppsRunInOSOnly().size(),
            ExtensionAppsRunInOSOnlyAllowlistSizeForTest());
  ASSERT_TRUE(IsIdenticalList(extensions::GetExtensionAppsRunInOSOnly(),
                              mojo_keeplist->extension_apps_run_in_os_only));
}

IN_PROC_BROWSER_TEST_F(ExtensionKeeplistTest, PerfettoNotInKeepListByDefault) {
  ASSERT_FALSE(ash::switches::IsAshDebugBrowserEnabled());
  ASSERT_FALSE(
      base::Contains(extensions::GetExtensionsRunInOSAndStandaloneBrowser(),
                     extension_misc::kPerfettoUIExtensionId));
  ASSERT_FALSE(base::Contains(
      extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser(),
      extension_misc::kPerfettoUIExtensionId));
}

class ExtensionKeeplistAllowPerfettoTest
    : public AshRequiresLacrosExtensionApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kEnableAshDebugBrowser);
  }
  test::AshBrowserTestStarter ash_starter_;
};

IN_PROC_BROWSER_TEST_F(ExtensionKeeplistAllowPerfettoTest, PerfettoInKeepList) {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }

  ASSERT_TRUE(ash::switches::IsAshDebugBrowserEnabled());
  ASSERT_TRUE(
      base::Contains(extensions::GetExtensionsRunInOSAndStandaloneBrowser(),
                     extension_misc::kPerfettoUIExtensionId));
  ASSERT_TRUE(base::Contains(
      extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser(),
      extension_misc::kPerfettoUIExtensionId));

  // Get the Ash extension keeplist data from Lacros.
  base::test::TestFuture<crosapi::mojom::ExtensionKeepListPtr> future;
  GetStandaloneBrowserTestController()->GetExtensionKeeplist(
      future.GetCallback());
  auto mojo_keeplist = future.Take();

  ASSERT_EQ(extensions::GetExtensionsRunInOSAndStandaloneBrowser().size(),
            ExtensionsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_TRUE(IsIdenticalList(
      extensions::GetExtensionsRunInOSAndStandaloneBrowser(),
      mojo_keeplist->extensions_run_in_os_and_standalonebrowser));
}
}  // namespace extensions

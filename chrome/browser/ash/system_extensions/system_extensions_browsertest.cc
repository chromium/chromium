// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kSystemExtensionsProfileDirectory[] = "SystemExtensions";

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

constexpr char kTestSystemExtensionIndexURL[] =
    "chrome-untrusted://system-extension-echo-01020304/html/index.html";

constexpr char kTestSystemExtensionWrongURL[] =
    "chrome-untrusted://system-extension-echo-01020304/html/wrong.html";

constexpr char kTestSystemExtensionEmptyPathURL[] =
    "chrome-untrusted://system-extension-echo-01020304/";

base::FilePath GetBasicSystemExtensionDir() {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  return test_dir.Append("system_extensions").Append("basic_system_extension");
}

// Creates fake resources in the directory where the System Extension would
// be installed.
void CreateFakeSystemExtensionResources(
    const base::FilePath& profile_path,
    const SystemExtensionId& kTestSystemExtensionId) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath system_extensions_dir =
      profile_path.Append(kSystemExtensionsProfileDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_extensions_dir));

  ASSERT_TRUE(base::CopyDirectory(GetBasicSystemExtensionDir(),
                                  system_extensions_dir, true));

  base::FilePath system_extension_dir = system_extensions_dir.Append(
      SystemExtension::IdToString(kTestSystemExtensionId));
  ASSERT_TRUE(base::Move(system_extensions_dir.Append("basic_system_extension"),
                         system_extension_dir));
}

class SystemExtensionsBrowserTest : public InProcessBrowserTest {
 public:
  SystemExtensionsBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSystemExtensions);
  }

  ~SystemExtensionsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchPath(chromeos::switches::kInstallSystemExtension,
                                   GetBasicSystemExtensionDir());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, ExtensionInstalled) {
  auto* provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider->install_manager();

  base::RunLoop run_loop;
  install_manager.on_command_line_install_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  auto extension_ids = install_manager.GetSystemExtensionIds();
  EXPECT_EQ(std::vector<SystemExtensionId>({kTestSystemExtensionId}),
            extension_ids);
  EXPECT_TRUE(install_manager.GetSystemExtensionById(kTestSystemExtensionId));

  // TODO(ortuno): Actually move resources instead of faking them.
  CreateFakeSystemExtensionResources(browser()->profile()->GetPath(),
                                     kTestSystemExtensionId);
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(kTestSystemExtensionIndexURL)));
    EXPECT_EQ(u"SystemExtension", tab->GetTitle());
  }
  {
    // Check that navigating to non-existing resources doesn't crash the
    // browser.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(kTestSystemExtensionWrongURL)));
    content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  }
  {
    // Check that navigating to a directory, like the root directory, doesn't
    // crash the browser.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(kTestSystemExtensionEmptyPathURL)));
    content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  }
}

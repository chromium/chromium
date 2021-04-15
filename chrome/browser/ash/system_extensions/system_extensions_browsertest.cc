// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kSystemExtensionsProfileDirectory[] = "SystemExtensions";

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

constexpr char kTestSystemExtensionIndex[] = R"(
<title>SystemExtension</title>
<h1>I'm a System Extension!</h1>
)";

constexpr char kTestSystemExtensionIndexURL[] =
    "chrome-untrusted://system-extension-echo-1234/html/index.html";

constexpr char kTestSystemExtensionWrongURL[] =
    "chrome-untrusted://system-extension-echo-1234/html/wrong.html";

constexpr char kTestSystemExtensionEmptyPathURL[] =
    "chrome-untrusted://system-extension-echo-1234/";

// Creates fake resources in the directory where the System Extension would
// be installed.
void CreateFakeSystemExtensionResources(
    const base::FilePath& profile_path,
    const SystemExtensionId& kTestSystemExtensionId) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath system_extensions_dir =
      profile_path.Append(kSystemExtensionsProfileDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_extensions_dir));

  base::FilePath system_extension_dir = system_extensions_dir.Append(
      SystemExtension::IdToString(kTestSystemExtensionId));
  ASSERT_TRUE(base::CreateDirectory(system_extension_dir));

  base::FilePath html_resources_dir = system_extension_dir.Append("html");
  ASSERT_TRUE(base::CreateDirectory(html_resources_dir));

  base::FilePath index_html = html_resources_dir.Append("index.html");
  ASSERT_TRUE(base::WriteFile(index_html, kTestSystemExtensionIndex));
}

class SystemExtensionsBrowserTest : public InProcessBrowserTest {
 public:
  SystemExtensionsBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSystemExtensions);
  }

  ~SystemExtensionsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, ExtensionInstalled) {
  auto* provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider->install_manager();
  auto extension_ids = install_manager.GetSystemExtensionIds();
  EXPECT_EQ(std::vector<SystemExtensionId>({kTestSystemExtensionId}),
            extension_ids);
  EXPECT_TRUE(install_manager.GetSystemExtensionById(kTestSystemExtensionId));

  // TODO(calamity): Actually create resources instead of faking them.
  CreateFakeSystemExtensionResources(browser()->profile()->GetPath(),
                                     kTestSystemExtensionId);
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  {
    ui_test_utils::NavigateToURL(browser(), GURL(kTestSystemExtensionIndexURL));
    EXPECT_EQ(u"SystemExtension", tab->GetTitle());
  }
  {
    // Check that navigating to non-existing resources doesn't crash the
    // browser.
    ui_test_utils::NavigateToURL(browser(), GURL(kTestSystemExtensionWrongURL));
    content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  }
  {
    // Check that navigating to a directory, like the root directory, doesn't
    // crash the browser.
    ui_test_utils::NavigateToURL(browser(),
                                 GURL(kTestSystemExtensionEmptyPathURL));
    content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  }
}

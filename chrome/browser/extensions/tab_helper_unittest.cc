// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/test/test_extension_dir.h"
#include "url/origin.h"

namespace extensions {

class TabHelperUnitTest : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    std::unique_ptr<content::WebContents> web_contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    web_contents_tester_ = content::WebContentsTester::For(web_contents.get());
    TabHelper::CreateForWebContents(web_contents.get());
    tab_helper_ = TabHelper::FromWebContents(web_contents.get());
    browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                    true);

    permissions_manager_ = PermissionsManager::Get(profile());
  }

  void TearDown() override {
    // Remove any tabs in the tab strip to avoid test crashes.
    if (browser_) {
      while (!browser_->tab_strip_model()->empty()) {
        browser_->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
      }
    }

    ExtensionServiceTestBase::TearDown();
  }

  Browser* browser() {
    if (!browser_) {
      Browser::CreateParams params(profile(), true);
      browser_window_ = std::make_unique<TestBrowserWindow>();
      params.window = browser_window_.get();
      browser_.reset(Browser::Create(params));
    }
    return browser_.get();
  }

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  TabHelper* tab_helper() { return tab_helper_; }

  PermissionsManager* permissions_manager() { return permissions_manager_; }

 private:
  // The browser and accompaying window.
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestBrowserWindow> browser_window_;

  raw_ptr<content::WebContentsTester, DanglingUntriaged> web_contents_tester_;
  raw_ptr<TabHelper, DanglingUntriaged> tab_helper_;
  raw_ptr<PermissionsManager> permissions_manager_;
};

TEST_F(TabHelperUnitTest, ClearsExtensionOnUnload) {
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("hosted_app"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  tab_helper()->SetExtensionApp(extension);
  EXPECT_EQ(extension->id(), tab_helper()->GetExtensionAppId());
  EXPECT_TRUE(tab_helper()->is_app());
  service()->UnloadExtension(extension->id(),
                             UnloadedExtensionReason::TERMINATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ExtensionId(), tab_helper()->GetExtensionAppId());
}

TEST_F(TabHelperUnitTest, ReloadRequired_BlockAllExtensions) {
  static constexpr char kManifest[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["<all_urls>"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  const Extension* extension =
      PackAndInstallCRX(test_dir.UnpackedPath(), INSTALL_NEW);
  ASSERT_TRUE(extension);

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  // By default, user can customize extension's site access.
  EXPECT_EQ(permissions_manager()->GetUserSiteSetting(url::Origin::Create(url)),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // Reload is required when user wants to block all extensions and any
  // extension loses site access.
  tab_helper()->SetReloadRequired(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_TRUE(tab_helper()->IsReloadRequired());

  // Navigating to another url restores the reload required value.
  const GURL other_url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(other_url);
  EXPECT_FALSE(tab_helper()->IsReloadRequired());
}

TEST_F(TabHelperUnitTest, ReloadRequired_CustomizeByExtension) {
  static constexpr char kManifest[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  const Extension* extension =
      PackAndInstallCRX(test_dir.UnpackedPath(), INSTALL_NEW);
  ASSERT_TRUE(extension);

  // Change site setting to "block all extensions", so we can test whether a
  // reload will be required for "customize by extension".
  const GURL url("http://www.example.com");
  auto origin = url::Origin::Create(url);
  permissions_manager()->UpdateUserSiteSetting(
      origin, PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  web_contents_tester()->NavigateAndCommit(url);

  // Reload is required when user wants to customize by extension, regardless
  // of whether the extension requires site access.
  tab_helper()->SetReloadRequired(
      PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(tab_helper()->IsReloadRequired());

  // Navigating to another url restores the reload required value.
  const GURL other_url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(other_url);
  EXPECT_FALSE(tab_helper()->IsReloadRequired());
}

}  // namespace extensions

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/test_extension_dir.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#endif
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class TabHelperUnitTest : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents_.get());
    TabHelper::CreateForWebContents(web_contents_.get());
    tab_helper_ = TabHelper::FromWebContents(web_contents_.get());
    permissions_manager_ = PermissionsManager::Get(profile());
  }

  void TearDown() override {
    permissions_manager_ = nullptr;
    tab_helper_ = nullptr;
    web_contents_tester_ = nullptr;
    web_contents_.reset();
    ExtensionServiceTestWithInstall::TearDown();
  }

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  TabHelper* tab_helper() { return tab_helper_; }

  PermissionsManager* permissions_manager() { return permissions_manager_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<content::WebContentsTester> web_contents_tester_ = nullptr;
  raw_ptr<TabHelper> tab_helper_ = nullptr;
  raw_ptr<PermissionsManager> permissions_manager_ = nullptr;
};

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

TEST_F(TabHelperUnitTest, SetReloadRequired_AccumulatesMultipleExtensions) {
  static constexpr char kManifest1[] =
      R"({
           "name": "Extension 1",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["<all_urls>"]
         })";
  static constexpr char kManifest2[] =
      R"({
           "name": "Extension 2",
           "manifest_version": 3,
           "version": "0.1",
           "host_permissions": ["<all_urls>"]
         })";
  TestExtensionDir test_dir1;
  test_dir1.WriteManifest(kManifest1);
  const Extension* extension1 =
      PackAndInstallCRX(test_dir1.UnpackedPath(), INSTALL_NEW);

  TestExtensionDir test_dir2;
  test_dir2.WriteManifest(kManifest2);
  const Extension* extension2 =
      PackAndInstallCRX(test_dir2.UnpackedPath(), INSTALL_NEW);

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  web_contents->WasHidden();
  TabHelper::CreateForWebContents(web_contents.get());
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents.get());
  ASSERT_TRUE(tab_helper);

  tab_helper->SetReloadRequired(std::vector<const Extension*>{extension1});
  EXPECT_TRUE(tab_helper->IsReloadRequired());

  // Adding a second extension on an unvisited background tab should accumulate
  // rather than overwrite.
  tab_helper->SetReloadRequired(std::vector<const Extension*>{extension2});
  EXPECT_TRUE(tab_helper->IsReloadRequired());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(TabHelperUnitTest,
       ShowReloadBubbleForAllExtensions_AndroidTabModelList) {
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

  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  auto web_contents_a =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto web_contents_b =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  TabHelper::CreateForWebContents(web_contents_a.get());
  TabHelper::CreateForWebContents(web_contents_b.get());

  const GURL url("http://www.example.com");
  content::WebContentsTester::For(web_contents_a.get())->NavigateAndCommit(url);
  content::WebContentsTester::For(web_contents_b.get())->NavigateAndCommit(url);

  tab_model.SetWebContentsList({web_contents_a.get(), web_contents_b.get()});

  ChromeExtensionsBrowserClient::Get()->ShowReloadBubbleForAllExtensions(
      {extension}, web_contents_a.get());

  TabHelper* tab_helper_a = TabHelper::FromWebContents(web_contents_a.get());
  ASSERT_TRUE(tab_helper_a);
  EXPECT_TRUE(tab_helper_a->IsReloadRequired());

  TabHelper* tab_helper_b = TabHelper::FromWebContents(web_contents_b.get());
  ASSERT_TRUE(tab_helper_b);
  EXPECT_TRUE(tab_helper_b->IsReloadRequired());

  TabModelList::RemoveTabModel(&tab_model);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(TabHelperUnitTest, OnExtensionUnloaded_ClearsReloadRequired) {
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

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  web_contents->WasHidden();
  TabHelper::CreateForWebContents(web_contents.get());
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents.get());
  ASSERT_TRUE(tab_helper);

  tab_helper->SetReloadRequired(std::vector<const Extension*>{extension});
  EXPECT_TRUE(tab_helper->IsReloadRequired());

  // Unloading the extension should remove it from pending reload extensions and
  // clear reload_required_.
  registrar()->DisableExtension(
      extension->id(), DisableReasonSet{disable_reason::DISABLE_USER_ACTION});
  EXPECT_FALSE(tab_helper->IsReloadRequired());
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

void SetImage(gfx::Image* image_out,
              base::OnceClosure quit_closure,
              const gfx::Image& image_in) {
  *image_out = image_in;
  std::move(quit_closure).Run();
}

class ExtensionInstallPromptUnitTest : public testing::Test {
 public:
  ExtensionInstallPromptUnitTest() = default;

  ExtensionInstallPromptUnitTest(const ExtensionInstallPromptUnitTest&) =
      delete;
  ExtensionInstallPromptUnitTest& operator=(
      const ExtensionInstallPromptUnitTest&) = delete;

  ~ExtensionInstallPromptUnitTest() override = default;

  // testing::Test:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }
  void TearDown() override { profile_.reset(); }

  Profile* profile() { return profile_.get(); }

 private:
  // This test does not create a root window. Because of this,
  // ScopedDisableRootChecking needs to be used (which disables the root window
  // check).
  test::ScopedDisableRootChecking disable_root_checking_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

using ShowDialogTestFuture =
    base::test::TestFuture<std::unique_ptr<ExtensionInstallPromptShowParams>,
                           ExtensionInstallPrompt::DoneCallback,
                           std::unique_ptr<InstallPromptData>>;

}  // namespace

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsPermissionWarnings) {
  APIPermissionSet api_permissions;
  api_permissions.insert(mojom::APIPermissionID::kTab);
  std::unique_ptr<const PermissionSet> permission_set(
      new PermissionSet(std::move(api_permissions), ManifestPermissionSet(),
                        URLPatternSet(), URLPatternSet()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::DictValue()
                           .Set("name", "foo")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Random Ext"))
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()),
                                std::make_unique<InstallPromptData>(
                                    InstallPromptData::PERMISSIONS_PROMPT));
  ShowDialogTestFuture show_dialog_future;

  prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension.get(),
                    nullptr, std::move(permission_set),
                    show_dialog_future.GetRepeatingCallback());

  auto [params, done_callback, install_prompt] = show_dialog_future.Take();
  ASSERT_TRUE(install_prompt.get());
  EXPECT_EQ(1u, install_prompt->GetPermissionCount());
}

using ExtensionInstallPromptTestWithService = ExtensionServiceTestWithInstall;

TEST_F(ExtensionInstallPromptTestWithService, ExtensionInstallPromptIconsTest) {
  // This test does not create a root window. Because of this,
  // ScopedDisableRootChecking needs to be used (which disables the root window
  // check).
  test::ScopedDisableRootChecking disable_root_checking;
  InitializeEmptyExtensionService();

  const Extension* extension = PackAndInstallCRX(
      data_dir().AppendASCII("simple_with_icon"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  std::vector<ImageLoader::ImageRepresentation> image_rep(
      1, ImageLoader::ImageRepresentation(
             IconsInfo::GetIconResource(extension,
                                        extension_misc::EXTENSION_ICON_LARGE,
                                        ExtensionIconSet::Match::kBigger),
             ImageLoader::ImageRepresentation::NEVER_RESIZE, gfx::Size(),
             ui::k100Percent));
  base::test::TestFuture<void> image_future;
  gfx::Image image;
  ImageLoader::Get(browser_context())
      ->LoadImagesAsync(
          extension, image_rep,
          base::BindOnce(&SetImage, &image, image_future.GetCallback()));
  ASSERT_TRUE(image_future.Wait())
      << "LoadImagesAsync did not trigger the callback.";
  ASSERT_FALSE(image.IsEmpty());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr));
  {
    ExtensionInstallPrompt prompt(web_contents.get(),
                                  std::make_unique<InstallPromptData>(
                                      InstallPromptData::PERMISSIONS_PROMPT));
    ShowDialogTestFuture show_dialog_future;

    prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension,
                      nullptr,  // Force an icon fetch.
                      show_dialog_future.GetRepeatingCallback());

    auto [params, done_callback, install_prompt] = show_dialog_future.Take();
    EXPECT_TRUE(gfx::BitmapsAreEqual(install_prompt->icon().AsBitmap(),
                                     image.AsBitmap()));
  }

  {
    ExtensionInstallPrompt prompt(
        web_contents.get(),
        std::make_unique<InstallPromptData>(InstallPromptData::INSTALL_PROMPT));
    ShowDialogTestFuture show_dialog_future;

    gfx::ImageSkia app_icon = util::GetDefaultAppIcon();
    prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension,
                      app_icon.bitmap(),  // Use a different icon.
                      show_dialog_future.GetRepeatingCallback());

    auto [params, done_callback, install_prompt] = show_dialog_future.Take();
    EXPECT_TRUE(gfx::BitmapsAreEqual(install_prompt->icon().AsBitmap(),
                                     *app_icon.bitmap()));
  }
}

class ExtensionInstallPromptTestSiteAccessOptionsEnabled
    : public ExtensionInstallPromptUnitTest {
 public:
  ExtensionInstallPromptTestSiteAccessOptionsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionInstallSiteAccessOptions);
  }

 protected:
  std::unique_ptr<InstallPromptData> ShowPromptAndGetData(
      const Extension* extension) {
    content::TestWebContentsFactory web_contents_factory;
    ExtensionInstallPrompt prompt(
        web_contents_factory.CreateWebContents(profile()),
        std::make_unique<InstallPromptData>(InstallPromptData::INSTALL_PROMPT));
    ShowDialogTestFuture show_dialog_future;
    prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension,
                      nullptr, show_dialog_future.GetRepeatingCallback());
    auto [params, done_callback, install_prompt] = show_dialog_future.Take();
    return std::move(install_prompt);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       DoesntShowForNoHostsRequested) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("no_host").AddAPIPermission("tabs").Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  EXPECT_FALSE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       DoesntShowForFilteredNonRegularHostsOnly) {
  scoped_refptr<const Extension> favicon_only =
      ExtensionBuilder("favicon_only")
          .SetManifestVersion(2)
          .AddHostPermission("chrome://favicon/*")
          .Build();
  std::unique_ptr<InstallPromptData> favicon_prompt =
      ShowPromptAndGetData(favicon_only.get());
  EXPECT_FALSE(favicon_prompt->ShouldWithheldPermissionsOnDialogAccept());

  scoped_refptr<const Extension> file_only =
      ExtensionBuilder("file_only").AddHostPermission("file:///*").Build();
  std::unique_ptr<InstallPromptData> file_prompt =
      ShowPromptAndGetData(file_only.get());
  EXPECT_FALSE(file_prompt->ShouldWithheldPermissionsOnDialogAccept());
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       DoesntShowForWithholdingNotAllowed) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("all_hosts")
          .AddHostPermission("<all_urls>")
          .SetLocation(mojom::ManifestLocation::kExternalPolicy)
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  EXPECT_FALSE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  EXPECT_EQ(1u, install_prompt->GetPermissionCount());
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       DoesntShowForPlatformApp) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .AddHostPermission("<all_urls>")
          .AddAPIPermission("notifications")
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(app.get());
  EXPECT_FALSE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  ASSERT_EQ(1u, install_prompt->GetPermissionCount());
  EXPECT_EQ(u"Display notifications", install_prompt->GetPermission(0));
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       DoesntShowWhenAPIPermissionImpliesAllHosts) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("proxy_and_host")
          .AddAPIPermission("proxy")
          .AddHostPermission("https://example.com/*")
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  EXPECT_FALSE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  ASSERT_EQ(1u, install_prompt->GetPermissionCount());
  EXPECT_EQ(u"Read and change all your data on all websites",
            install_prompt->GetPermission(0));
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       SiteAccessOptions_AllSites) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("all_sites")
          .AddHostPermission("<all_urls>")
          .AddAPIPermission("notifications")
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  ASSERT_TRUE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  EXPECT_TRUE(install_prompt->has_all_hosts_permission());

  // GetPermissionCount() still includes all permissions, while
  // GetNonHostPermissions() contains only the non-host permission.
  EXPECT_EQ(2u, install_prompt->GetPermissionCount());
  ASSERT_EQ(1u, install_prompt->GetNonHostPermissionCount());
  EXPECT_EQ(u"Display notifications",
            install_prompt->GetNonHostPermissions().permissions[0]);

  EXPECT_EQ(u"Read and change your data",
            install_prompt->GetSiteAccessHeader());
  EXPECT_EQ(u"Only when you click the extension",
            install_prompt->GetSiteAccessOnClickOptionText());
  EXPECT_EQ(u"Grants temporary access to your data on the site",
            install_prompt->GetSiteAccessOnClickOptionSubtext());
  EXPECT_EQ(u"Always on all sites",
            install_prompt->GetSiteAccessAlwaysOptionText());
  EXPECT_EQ(u"Grants permanent access to your data on all sites",
            install_prompt->GetSiteAccessAlwaysOptionSubtext());
  EXPECT_TRUE(install_prompt->GetVisibleHostPermissions().empty());
  EXPECT_TRUE(install_prompt->GetCollapsedHostPermissions().empty());
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       SiteAccessOptions_AllSitesWithTabsShowsBrowsingHistoryInNonHost) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("all_sites_and_tabs")
          .AddHostPermission("<all_urls>")
          .AddAPIPermission("tabs")
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  ASSERT_TRUE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  EXPECT_TRUE(install_prompt->has_all_hosts_permission());
  ASSERT_EQ(1u, install_prompt->GetNonHostPermissionCount());
  EXPECT_EQ(u"Read your browsing history",
            install_prompt->GetNonHostPermissions().permissions[0]);
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       SiteAccessOptions_FaviconAndRegularHostKeepsFaviconInNonHost) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("favicon_and_host")
          .SetManifestVersion(2)
          .AddHostPermissions({"chrome://favicon/*", "https://example.com/*"})
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  ASSERT_TRUE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  EXPECT_FALSE(install_prompt->has_all_hosts_permission());
  ASSERT_EQ(1u, install_prompt->GetNonHostPermissionCount());
  EXPECT_EQ(u"Read the icons of the websites you visit",
            install_prompt->GetNonHostPermissions().permissions[0]);
  EXPECT_EQ(u"• example.com", install_prompt->GetVisibleHostPermissions());
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       SiteAccessOptions_ThreeOrFewerListedSites) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("three_sites")
          .AddHostPermissions({"https://calendar.google.com/*",
                               "https://*.figma.com/*",
                               "https://website.com/*"})
          .AddAPIPermission("downloads")
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  ASSERT_TRUE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  EXPECT_FALSE(install_prompt->has_all_hosts_permission());

  EXPECT_EQ(2u, install_prompt->GetPermissionCount());
  ASSERT_EQ(1u, install_prompt->GetNonHostPermissionCount());
  EXPECT_EQ(u"Manage your downloads",
            install_prompt->GetNonHostPermissions().permissions[0]);

  EXPECT_EQ(u"Always on the listed sites",
            install_prompt->GetSiteAccessAlwaysOptionText());
  EXPECT_EQ(u"Grants permanent access to your data on the sites listed below",
            install_prompt->GetSiteAccessAlwaysOptionSubtext());
  EXPECT_EQ(u"• All figma.com sites\n• calendar.google.com\n• website.com",
            install_prompt->GetVisibleHostPermissions());
  EXPECT_TRUE(install_prompt->GetCollapsedHostPermissions().empty());
}

TEST_F(ExtensionInstallPromptTestSiteAccessOptionsEnabled,
       SiteAccessOptions_MoreThanThreeListedSites) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("five_sites")
          .AddHostPermissions({"https://a.com/*", "https://b.com/*",
                               "https://c.com/*", "https://d.com/*",
                               "https://e.com/*"})
          .Build();
  std::unique_ptr<InstallPromptData> install_prompt =
      ShowPromptAndGetData(extension.get());
  ASSERT_TRUE(install_prompt->ShouldWithheldPermissionsOnDialogAccept());
  EXPECT_FALSE(install_prompt->has_all_hosts_permission());
  EXPECT_EQ(1u, install_prompt->GetPermissionCount());
  EXPECT_EQ(0u, install_prompt->GetNonHostPermissionCount());

  EXPECT_EQ(u"Always on the listed sites",
            install_prompt->GetSiteAccessAlwaysOptionText());
  EXPECT_EQ(u"Grants permanent access to your data on the sites listed below",
            install_prompt->GetSiteAccessAlwaysOptionSubtext());
  EXPECT_EQ(u"• a.com\n• b.com\n• c.com",
            install_prompt->GetVisibleHostPermissions());
  EXPECT_EQ(u"• d.com\n• e.com", install_prompt->GetCollapsedHostPermissions());
}

}  // namespace extensions

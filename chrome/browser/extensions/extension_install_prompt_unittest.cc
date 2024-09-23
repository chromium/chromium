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

  ~ExtensionInstallPromptUnitTest() override {}

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
                           std::unique_ptr<ExtensionInstallPrompt::Prompt>>;

}  // namespace

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsPermissionWarnings) {
  APIPermissionSet api_permissions;
  api_permissions.insert(mojom::APIPermissionID::kTab);
  std::unique_ptr<const PermissionSet> permission_set(
      new PermissionSet(std::move(api_permissions), ManifestPermissionSet(),
                        URLPatternSet(), URLPatternSet()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "foo")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Random Ext"))
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  ShowDialogTestFuture show_dialog_future;

  prompt.ShowDialog(
      ExtensionInstallPrompt::DoneCallback(), extension.get(), nullptr,
      std::make_unique<ExtensionInstallPrompt::Prompt>(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT),
      std::move(permission_set), show_dialog_future.GetRepeatingCallback());

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
    ExtensionInstallPrompt prompt(web_contents.get());
    ShowDialogTestFuture show_dialog_future;

    prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension,
                      nullptr,  // Force an icon fetch.
                      show_dialog_future.GetRepeatingCallback());

    auto [params, done_callback, install_prompt] = show_dialog_future.Take();
    EXPECT_TRUE(gfx::BitmapsAreEqual(install_prompt->icon().AsBitmap(),
                                     image.AsBitmap()));
  }

  {
    ExtensionInstallPrompt prompt(web_contents.get());
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

class ExtensionInstallPromptTestWithholdingAllowed
    : public ExtensionInstallPromptUnitTest {
 public:
  ExtensionInstallPromptTestWithholdingAllowed() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExtensionInstallPromptTestWithholdingAllowed,
       PromptShouldShowWithholdingUI) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  ShowDialogTestFuture show_dialog_future;

  prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension.get(),
                    nullptr, show_dialog_future.GetRepeatingCallback());

  auto [params, done_callback, install_prompt] = show_dialog_future.Take();
  EXPECT_EQ(install_prompt->ShouldWithheldPermissionsOnDialogAccept(), true);
}

TEST_F(ExtensionInstallPromptTestWithholdingAllowed,
       DoesntShowForNoHostsRequested) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("no_host").AddAPIPermission("tabs").Build();
  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  ShowDialogTestFuture show_dialog_future;

  prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension.get(),
                    nullptr, show_dialog_future.GetRepeatingCallback());

  auto [params, done_callback, install_prompt] = show_dialog_future.Take();
  EXPECT_EQ(install_prompt->ShouldWithheldPermissionsOnDialogAccept(), false);
}

TEST_F(ExtensionInstallPromptTestWithholdingAllowed,
       DoesntShowForWithholdingNotAllowed) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("all_hosts")
          .AddHostPermission("<all_urls>")
          .SetLocation(mojom::ManifestLocation::kExternalPolicy)
          .Build();
  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  ShowDialogTestFuture show_dialog_future;

  prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension.get(),
                    nullptr, show_dialog_future.GetRepeatingCallback());

  auto [params, done_callback, install_prompt] = show_dialog_future.Take();
  EXPECT_EQ(install_prompt->ShouldWithheldPermissionsOnDialogAccept(), false);
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
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
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

namespace extensions {

namespace {

void VerifyPromptIconCallback(
    const base::Closure& quit_closure,
    const SkBitmap& expected_bitmap,
    ExtensionInstallPromptShowParams* params,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  EXPECT_TRUE(gfx::BitmapsAreEqual(prompt->icon().AsBitmap(), expected_bitmap));
  quit_closure.Run();
}

void VerifyPromptPermissionsCallback(
    const base::Closure& quit_closure,
    size_t regular_permissions_count,
    ExtensionInstallPromptShowParams* params,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> install_prompt) {
  ASSERT_TRUE(install_prompt.get());
  EXPECT_EQ(regular_permissions_count, install_prompt->GetPermissionCount());
  quit_closure.Run();
}

void SetImage(gfx::Image* image_out,
              const base::Closure& quit_closure,
              const gfx::Image& image_in) {
  *image_out = image_in;
  quit_closure.Run();
}

class ExtensionInstallPromptUnitTest : public testing::Test {
 public:
  ExtensionInstallPromptUnitTest() {}
  ~ExtensionInstallPromptUnitTest() override {}

  // testing::Test:
  void SetUp() override {
    profile_.reset(new TestingProfile());
  }
  void TearDown() override {
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPromptUnitTest);
};

}  // namespace

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsPermissionWarnings) {
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermission::kTab);
  std::unique_ptr<const PermissionSet> permission_set(
      new PermissionSet(std::move(api_permissions), ManifestPermissionSet(),
                        URLPatternSet(), URLPatternSet()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "foo")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Random Ext")
                           .Build())
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;
  prompt.ShowDialog(
      ExtensionInstallPrompt::DoneCallback(), extension.get(), nullptr,
      std::make_unique<ExtensionInstallPrompt::Prompt>(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT),
      std::move(permission_set),
      base::Bind(&VerifyPromptPermissionsCallback, run_loop.QuitClosure(),
                 1u));  // |regular_permissions_count|.
  run_loop.Run();
}

TEST_F(ExtensionInstallPromptUnitTest,
       DelegatedPromptShowsOptionalPermissions) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "foo")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Random Ext")
                           .Set("permissions",
                                ListBuilder().Append("clipboardRead").Build())
                           .Set("optional_permissions",
                                ListBuilder().Append("tabs").Build())
                           .Build())
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;

  std::unique_ptr<ExtensionInstallPrompt::Prompt> sub_prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::DELEGATED_PERMISSIONS_PROMPT));
  sub_prompt->set_delegated_username("Username");
  prompt.ShowDialog(
      ExtensionInstallPrompt::DoneCallback(), extension.get(), nullptr,
      std::move(sub_prompt),
      base::Bind(&VerifyPromptPermissionsCallback, run_loop.QuitClosure(),
                 2u));  // |regular_permissions_count|.
  run_loop.Run();
}

using ExtensionInstallPromptTestWithService = ExtensionServiceTestWithInstall;

TEST_F(ExtensionInstallPromptTestWithService, ExtensionInstallPromptIconsTest) {
  InitializeEmptyExtensionService();

  const Extension* extension = PackAndInstallCRX(
      data_dir().AppendASCII("simple_with_icon"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  std::vector<ImageLoader::ImageRepresentation> image_rep(
      1, ImageLoader::ImageRepresentation(
             IconsInfo::GetIconResource(extension,
                                        extension_misc::EXTENSION_ICON_LARGE,
                                        ExtensionIconSet::MATCH_BIGGER),
             ImageLoader::ImageRepresentation::NEVER_RESIZE, gfx::Size(),
             ui::SCALE_FACTOR_100P));
  base::RunLoop image_loop;
  gfx::Image image;
  ImageLoader::Get(browser_context())
      ->LoadImagesAsync(
          extension, image_rep,
          base::BindOnce(&SetImage, &image, image_loop.QuitClosure()));
  image_loop.Run();
  ASSERT_FALSE(image.IsEmpty());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr));
  {
    ExtensionInstallPrompt prompt(web_contents.get());
    base::RunLoop run_loop;
    prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(), extension,
                      nullptr,  // Force an icon fetch.
                      base::Bind(&VerifyPromptIconCallback,
                                 run_loop.QuitClosure(), image.AsBitmap()));
    run_loop.Run();
  }

  {
    ExtensionInstallPrompt prompt(web_contents.get());
    base::RunLoop run_loop;
    gfx::ImageSkia app_icon = util::GetDefaultAppIcon();
    prompt.ShowDialog(ExtensionInstallPrompt::DoneCallback(),
                      extension,
                      app_icon.bitmap(),  // Use a different icon.
                      base::Bind(&VerifyPromptIconCallback,
                                 run_loop.QuitClosure(), *app_icon.bitmap()));
    run_loop.Run();
  }
}

}  // namespace extensions

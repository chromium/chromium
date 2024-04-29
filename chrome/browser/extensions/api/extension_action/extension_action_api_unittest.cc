// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {
namespace {

class ExtensionActionAPIUnitTest
    : public ExtensionServiceTestWithInstall,
      public ::testing::WithParamInterface<ActionInfo::Type> {};

// Test that extensions can provide icons of arbitrary sizes in the manifest.
TEST_P(ExtensionActionAPIUnitTest, MultiIcons) {
  InitializeEmptyExtensionService();

  constexpr char kManifestTemplate[] =
      R"({
           "name": "A test extension that tests multiple browser action icons",
           "version": "1.0",
           "manifest_version": %d,
           "%s": {
             "default_icon": {
               "19": "icon19.png",
               "24": "icon24.png",
               "31": "icon24.png",
               "38": "icon38.png"
             }
           }
         })";

  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(GetParam()),
      ActionInfo::GetManifestKeyForActionType(GetParam())));

  {
    std::string icon_file_content;
    base::FilePath icon_path = data_dir().AppendASCII("icon1.png");
    EXPECT_TRUE(base::ReadFileToString(icon_path, &icon_file_content));
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon19.png"),
                                 icon_file_content);
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon24.png"),
                                 icon_file_content);
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("icon38.png"),
                                 icon_file_content);
  }

  const Extension* extension =
      PackAndInstallCRX(test_extension_dir.UnpackedPath(), INSTALL_NEW);
  EXPECT_TRUE(extension->install_warnings().empty());
  const ActionInfo* action_info = GetActionInfoOfType(*extension, GetParam());
  ASSERT_TRUE(action_info);

  const ExtensionIconSet& icons = action_info->default_icon;

  EXPECT_EQ(4u, icons.map().size());
  EXPECT_EQ("icon19.png", icons.Get(19, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icon24.png", icons.Get(24, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icon24.png", icons.Get(31, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icon38.png", icons.Get(38, ExtensionIconSet::Match::kExactly));
}

// Test that localization in the manifest properly applies to the "action"
// title.
TEST_P(ExtensionActionAPIUnitTest, ActionLocalization) {
  InitializeEmptyExtensionService();

  TestExtensionDir test_dir;
  constexpr char kManifest[] =
      R"({
           "name": "Some extension",
           "version": "3.0",
           "manifest_version": %d,
           "default_locale": "en",
           "%s": { "default_title": "__MSG_default_action_title__" }
         })";
  test_dir.WriteManifest(
      base::StringPrintf(kManifest, GetManifestVersionForActionType(GetParam()),
                         ActionInfo::GetManifestKeyForActionType(GetParam())));

  constexpr char kMessages[] =
      R"({
           "default_action_title": {
             "message": "An Action Title!"
           }
         })";
  {
    // TODO(crbug.com/40151844): It's a bit clunky to write to nested
    // files in a TestExtensionDir. It'd be nice to provide better support for
    // this.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath locales = test_dir.UnpackedPath().AppendASCII("_locales");
    base::FilePath locales_en = locales.AppendASCII("en");
    base::FilePath messages_path = locales_en.AppendASCII("messages.json");
    ASSERT_TRUE(base::CreateDirectory(locales));
    ASSERT_TRUE(base::CreateDirectory(locales_en));
    ASSERT_TRUE(base::WriteFile(messages_path, kMessages));
  }

  const Extension* extension =
      PackAndInstallCRX(test_dir.UnpackedPath(), INSTALL_NEW);
  ASSERT_TRUE(extension);

  auto* action_manager = ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  EXPECT_EQ("An Action Title!",
            action->GetTitle(ExtensionAction::kDefaultTabId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionActionAPIUnitTest,
                         testing::Values(ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kPage,
                                         ActionInfo::Type::kAction));

}  // namespace
}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shortcut.h"

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/test/test_file_util.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

static const char kFileContents[] = "This is a target.";
static const char kFileContents2[] = "This is another target.";

class ShortcutTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_2_.CreateUniqueTempDir());

    link_file_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("My Link.lnk"));

    // Shortcut 1's properties
    {
      const FilePath target_file(
          temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Target 1.txt")));
      WriteFile(target_file, kFileContents, base::size(kFileContents));

      link_properties_.set_target(target_file);
      link_properties_.set_working_dir(temp_dir_.GetPath());
      link_properties_.set_arguments(L"--magic --awesome");
      link_properties_.set_description(L"Chrome is awesome.");
      link_properties_.set_icon(link_properties_.target, 4);
      link_properties_.set_app_id(L"Chrome");
      link_properties_.set_dual_mode(false);

      // The CLSID below was randomly selected.
      static constexpr CLSID toast_activator_clsid = {
          0x08d401c2,
          0x3f79,
          0x41d8,
          {0x89, 0xd0, 0x99, 0x25, 0xee, 0x16, 0x28, 0x63}};
      link_properties_.set_toast_activator_clsid(toast_activator_clsid);
    }

    // Shortcut 2's properties (all different from properties of shortcut 1).
    {
      const FilePath target_file_2(
          temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Target 2.txt")));
      WriteFile(target_file_2, kFileContents2, base::size(kFileContents2));

      FilePath icon_path_2;
      CreateTemporaryFileInDir(temp_dir_.GetPath(), &icon_path_2);

      link_properties_2_.set_target(target_file_2);
      link_properties_2_.set_working_dir(temp_dir_2_.GetPath());
      link_properties_2_.set_arguments(L"--super --crazy");
      link_properties_2_.set_description(L"The best in the west.");
      link_properties_2_.set_icon(icon_path_2, 0);
      link_properties_2_.set_app_id(L"Chrome.UserLevelCrazySuffix");
      link_properties_2_.set_dual_mode(true);
      link_properties_2_.set_toast_activator_clsid(CLSID_NULL);
    }
  }

  ScopedCOMInitializer com_initializer_;
  ScopedTempDir temp_dir_;
  ScopedTempDir temp_dir_2_;

  // The link file to be created/updated in the shortcut tests below.
  FilePath link_file_;

  // Properties for the created shortcut.
  ShortcutProperties link_properties_;

  // Properties for the updated shortcut.
  ShortcutProperties link_properties_2_;
};

}  // namespace

TEST_F(ShortcutTest, CreateAndResolveShortcutProperties) {
  // Test all properties.
  FilePath file_1(temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Link1.lnk")));
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      file_1, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties properties_read_1;
  ASSERT_TRUE(ResolveShortcutProperties(
      file_1, ShortcutProperties::PROPERTIES_ALL, &properties_read_1));
  EXPECT_EQ(static_cast<unsigned>(ShortcutProperties::PROPERTIES_ALL),
            properties_read_1.options);
  ValidatePathsAreEqual(link_properties_.target, properties_read_1.target);
  ValidatePathsAreEqual(link_properties_.working_dir,
                        properties_read_1.working_dir);
  EXPECT_EQ(link_properties_.arguments, properties_read_1.arguments);
  EXPECT_EQ(link_properties_.description, properties_read_1.description);
  ValidatePathsAreEqual(link_properties_.icon, properties_read_1.icon);
  EXPECT_EQ(link_properties_.icon_index, properties_read_1.icon_index);
  EXPECT_EQ(link_properties_.app_id, properties_read_1.app_id);
  EXPECT_EQ(link_properties_.dual_mode, properties_read_1.dual_mode);
  EXPECT_EQ(link_properties_.toast_activator_clsid,
            properties_read_1.toast_activator_clsid);

  // Test simple shortcut with no special properties set.
  FilePath file_2(temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Link2.lnk")));
  ShortcutProperties only_target_properties;
  only_target_properties.set_target(link_properties_.target);
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      file_2, only_target_properties, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties properties_read_2;
  ASSERT_TRUE(ResolveShortcutProperties(
      file_2, ShortcutProperties::PROPERTIES_ALL, &properties_read_2));
  EXPECT_EQ(static_cast<unsigned>(ShortcutProperties::PROPERTIES_ALL),
            properties_read_2.options);
  ValidatePathsAreEqual(only_target_properties.target,
                        properties_read_2.target);
  ValidatePathsAreEqual(FilePath(), properties_read_2.working_dir);
  EXPECT_EQ(L"", properties_read_2.arguments);
  EXPECT_EQ(L"", properties_read_2.description);
  ValidatePathsAreEqual(FilePath(), properties_read_2.icon);
  EXPECT_EQ(0, properties_read_2.icon_index);
  EXPECT_EQ(L"", properties_read_2.app_id);
  EXPECT_FALSE(properties_read_2.dual_mode);
  EXPECT_EQ(CLSID_NULL, properties_read_2.toast_activator_clsid);
}

TEST_F(ShortcutTest, CreateAndResolveShortcut) {
  ShortcutProperties only_target_properties;
  only_target_properties.set_target(link_properties_.target);

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, only_target_properties, SHORTCUT_CREATE_ALWAYS));

  FilePath resolved_name;
  EXPECT_TRUE(ResolveShortcut(link_file_, &resolved_name, NULL));

  char read_contents[base::size(kFileContents)];
  base::ReadFile(resolved_name, read_contents, base::size(read_contents));
  EXPECT_STREQ(kFileContents, read_contents);
}

TEST_F(ShortcutTest, ResolveShortcutWithArgs) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  FilePath resolved_name;
  std::wstring args;
  EXPECT_TRUE(ResolveShortcut(link_file_, &resolved_name, &args));

  char read_contents[base::size(kFileContents)];
  base::ReadFile(resolved_name, read_contents, base::size(read_contents));
  EXPECT_STREQ(kFileContents, read_contents);
  EXPECT_EQ(link_properties_.arguments, args);
}

TEST_F(ShortcutTest, CreateShortcutWithOnlySomeProperties) {
  ShortcutProperties target_and_args_properties;
  target_and_args_properties.set_target(link_properties_.target);
  target_and_args_properties.set_arguments(link_properties_.arguments);

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, target_and_args_properties,
      SHORTCUT_CREATE_ALWAYS));

  ValidateShortcut(link_file_, target_and_args_properties);
}

TEST_F(ShortcutTest, CreateShortcutVerifyProperties) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ValidateShortcut(link_file_, link_properties_);
}

TEST_F(ShortcutTest, UpdateShortcutVerifyProperties) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_2_, SHORTCUT_UPDATE_EXISTING));

  ValidateShortcut(link_file_, link_properties_2_);
}

TEST_F(ShortcutTest, UpdateShortcutUpdateOnlyTargetAndResolve) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties update_only_target_properties;
  update_only_target_properties.set_target(link_properties_2_.target);

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, update_only_target_properties,
      SHORTCUT_UPDATE_EXISTING));

  ShortcutProperties expected_properties = link_properties_;
  expected_properties.set_target(link_properties_2_.target);
  ValidateShortcut(link_file_, expected_properties);

  FilePath resolved_name;
  EXPECT_TRUE(ResolveShortcut(link_file_, &resolved_name, NULL));

  char read_contents[base::size(kFileContents2)];
  base::ReadFile(resolved_name, read_contents, base::size(read_contents));
  EXPECT_STREQ(kFileContents2, read_contents);
}

TEST_F(ShortcutTest, UpdateShortcutMakeDualMode) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties make_dual_mode_properties;
  make_dual_mode_properties.set_dual_mode(true);

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, make_dual_mode_properties,
      SHORTCUT_UPDATE_EXISTING));

  ShortcutProperties expected_properties = link_properties_;
  expected_properties.set_dual_mode(true);
  ValidateShortcut(link_file_, expected_properties);
}

TEST_F(ShortcutTest, UpdateShortcutRemoveDualMode) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_2_, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties remove_dual_mode_properties;
  remove_dual_mode_properties.set_dual_mode(false);

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, remove_dual_mode_properties,
      SHORTCUT_UPDATE_EXISTING));

  ShortcutProperties expected_properties = link_properties_2_;
  expected_properties.set_dual_mode(false);
  ValidateShortcut(link_file_, expected_properties);
}

TEST_F(ShortcutTest, UpdateShortcutClearArguments) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties clear_arguments_properties;
  clear_arguments_properties.set_arguments(std::wstring());

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, clear_arguments_properties,
      SHORTCUT_UPDATE_EXISTING));

  ShortcutProperties expected_properties = link_properties_;
  expected_properties.set_arguments(std::wstring());
  ValidateShortcut(link_file_, expected_properties);
}

TEST_F(ShortcutTest, FailUpdateShortcutThatDoesNotExist) {
  ASSERT_FALSE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_UPDATE_EXISTING));
  ASSERT_FALSE(PathExists(link_file_));
}

TEST_F(ShortcutTest, ReplaceShortcutAllProperties) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_2_, SHORTCUT_REPLACE_EXISTING));

  ValidateShortcut(link_file_, link_properties_2_);
}

TEST_F(ShortcutTest, ReplaceShortcutSomeProperties) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  ShortcutProperties new_properties;
  new_properties.set_target(link_properties_2_.target);
  new_properties.set_arguments(link_properties_2_.arguments);
  new_properties.set_description(link_properties_2_.description);
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, new_properties, SHORTCUT_REPLACE_EXISTING));

  // Expect only properties in |new_properties| to be set, all other properties
  // should have been overwritten.
  ShortcutProperties expected_properties(new_properties);
  expected_properties.set_working_dir(FilePath());
  expected_properties.set_icon(FilePath(), 0);
  expected_properties.set_app_id(std::wstring());
  expected_properties.set_dual_mode(false);
  ValidateShortcut(link_file_, expected_properties);
}

TEST_F(ShortcutTest, FailReplaceShortcutThatDoesNotExist) {
  ASSERT_FALSE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_REPLACE_EXISTING));
  ASSERT_FALSE(PathExists(link_file_));
}

// Test that the old arguments remain on the replaced shortcut when not
// otherwise specified.
TEST_F(ShortcutTest, ReplaceShortcutKeepOldArguments) {
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_, SHORTCUT_CREATE_ALWAYS));

  // Do not explicitly set the arguments.
  link_properties_2_.options &=
      ~ShortcutProperties::PROPERTIES_ARGUMENTS;
  ASSERT_TRUE(CreateOrUpdateShortcutLink(
      link_file_, link_properties_2_, SHORTCUT_REPLACE_EXISTING));

  ShortcutProperties expected_properties(link_properties_2_);
  expected_properties.set_arguments(link_properties_.arguments);
  ValidateShortcut(link_file_, expected_properties);
}

}  // namespace win
}  // namespace base

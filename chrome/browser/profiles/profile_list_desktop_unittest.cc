// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_list_desktop.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_list.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace {

class MockObserver : public AvatarMenuObserver {
 public:
  MockObserver() : count_(0) {}
  ~MockObserver() override {}

  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override { ++count_; }

  int change_count() const { return count_; }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class ProfileListDesktopTest : public testing::Test {
 public:
  ProfileListDesktopTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {
  }

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());
  }

  AvatarMenu* GetAvatarMenu() {
    // Reset the MockObserver.
    mock_observer_.reset(new MockObserver());
    EXPECT_EQ(0, change_count());

    // Reset the menu.
    avatar_menu_.reset(new AvatarMenu(
        manager()->profile_attributes_storage(),
        mock_observer_.get(),
        NULL));
    avatar_menu_->RebuildMenu();
    EXPECT_EQ(0, change_count());
    return avatar_menu_.get();
  }

  TestingProfileManager* manager() { return &manager_; }

  void AddOmittedProfile(const std::string& name) {
    ProfileAttributesStorage* storage = manager()->profile_attributes_storage();
    storage->AddProfile(manager()->profiles_dir().AppendASCII(name),
                        ASCIIToUTF16(name), std::string(), base::string16(),
                        false, 0, "TEST_ID", EmptyAccountId());
  }

  int change_count() const { return mock_observer_->change_count(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;
  std::unique_ptr<MockObserver> mock_observer_;
  std::unique_ptr<AvatarMenu> avatar_menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListDesktopTest);
};

TEST_F(ProfileListDesktopTest, InitialCreation) {
  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

  AvatarMenu* menu = GetAvatarMenu();
  EXPECT_EQ(0, change_count());

  ASSERT_EQ(2U, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 1"), item1.name);

  const AvatarMenu::Item& item2 = menu->GetItemAt(1);
  EXPECT_EQ(1U, item2.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 2"), item2.name);
}

TEST_F(ProfileListDesktopTest, NoOmittedProfiles) {
  ProfileListDesktop
      profile_list_desktop(manager()->profile_attributes_storage());
  ProfileList* profile_list = &profile_list_desktop;

  // Profiles are stored and listed alphabetically.
  std::vector<std::string> profile_names = {"0 included",
                                            "1 included",
                                            "2 included",
                                            "3 included"};
  size_t profile_count = profile_names.size();

  // Add the profiles.
  for (const std::string profile_name : profile_names)
    manager()->CreateTestingProfile(profile_name);

  // Rebuild avatar menu.
  profile_list->RebuildMenu();
  ASSERT_EQ(profile_count, profile_list->GetNumberOfItems());

  // Verify contents in avatar menu.
  for (size_t i = 0u; i < profile_count; ++i) {
    const AvatarMenu::Item& item = profile_list->GetItemAt(i);
    EXPECT_EQ(i, item.menu_index);
    EXPECT_EQ(ASCIIToUTF16(profile_names[i]), item.name);
    EXPECT_EQ(i, profile_list->MenuIndexFromProfilePath(item.profile_path));
  }
}

TEST_F(ProfileListDesktopTest, WithOmittedProfiles) {
  ProfileListDesktop
      profile_list_desktop(manager()->profile_attributes_storage());
  ProfileList* profile_list = &profile_list_desktop;

  // Profiles are stored and listed alphabetically.
  std::vector<std::string> profile_names = {"0 omitted",
                                            "1 included",
                                            "2 omitted",
                                            "3 included",
                                            "4 included",
                                            "5 omitted",
                                            "6 included",
                                            "7 omitted"};

  // Add the profiles.
  std::vector<size_t> included_profile_indices;
  for (size_t i = 0u; i < profile_names.size(); ++i) {
    if (profile_names[i].find("included") != std::string::npos) {
      manager()->CreateTestingProfile(profile_names[i]);
      included_profile_indices.push_back(i);
    } else {
      AddOmittedProfile(profile_names[i]);
    }
  }

  // Rebuild avatar menu.
  size_t included_profile_count = included_profile_indices.size();
  profile_list->RebuildMenu();
  ASSERT_EQ(included_profile_count, profile_list->GetNumberOfItems());

  // Verify contents in avatar menu.
  for (size_t i = 0u; i < included_profile_count; ++i) {
    const AvatarMenu::Item& item = profile_list->GetItemAt(i);
    EXPECT_EQ(i, item.menu_index);
    EXPECT_EQ(ASCIIToUTF16(profile_names[included_profile_indices[i]]),
              item.name);
    EXPECT_EQ(i, profile_list->MenuIndexFromProfilePath(item.profile_path));
  }
}

TEST_F(ProfileListDesktopTest, ActiveItem) {
  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

  AvatarMenu* menu = GetAvatarMenu();
  ASSERT_EQ(2u, menu->GetNumberOfItems());
  // TODO(jeremy): Expand test to verify active profile index other than 0
  // crbug.com/100871
  ASSERT_EQ(0u, menu->GetActiveProfileIndex());
}

TEST_F(ProfileListDesktopTest, ModifyingNameResortsCorrectly) {
  std::string name1("Alpha");
  std::string name2("Beta");
  std::string newname1("Gamma");

  TestingProfile* profile1 = manager()->CreateTestingProfile(name1);
  manager()->CreateTestingProfile(name2);

  AvatarMenu* menu = GetAvatarMenu();
  EXPECT_EQ(0, change_count());

  ASSERT_EQ(2u, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0u);
  EXPECT_EQ(0u, item1.menu_index);
  EXPECT_EQ(ASCIIToUTF16(name1), item1.name);

  const AvatarMenu::Item& item2 = menu->GetItemAt(1u);
  EXPECT_EQ(1u, item2.menu_index);
  EXPECT_EQ(ASCIIToUTF16(name2), item2.name);

  // Change the name of the first profile, and this triggers the resorting of
  // the avatar menu.
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(manager()->profile_attributes_storage()->
                  GetProfileAttributesWithPath(profile1->GetPath(), &entry));
  entry->SetLocalProfileName(ASCIIToUTF16(newname1));
  EXPECT_EQ(1, change_count());

  // Now the first menu item should be named "beta", and the second be "gamma".
  const AvatarMenu::Item& item1next = menu->GetItemAt(0u);
  EXPECT_EQ(0u, item1next.menu_index);
  EXPECT_EQ(ASCIIToUTF16(name2), item1next.name);

  const AvatarMenu::Item& item2next = menu->GetItemAt(1u);
  EXPECT_EQ(1u, item2next.menu_index);
  EXPECT_EQ(ASCIIToUTF16(newname1), item2next.name);
}

TEST_F(ProfileListDesktopTest, ChangeOnNotify) {
  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

  AvatarMenu* menu = GetAvatarMenu();
  EXPECT_EQ(0, change_count());
  EXPECT_EQ(2u, menu->GetNumberOfItems());

  manager()->CreateTestingProfile("Test 3");

  // Three changes happened via the call to CreateTestingProfile: adding the
  // profile to the attributes storage, setting the user name (which rebuilds
  // the list of profiles after the name change) and changing the avatar.
  // On Windows, an extra change happens to set the shortcut name for the
  // profile.
  EXPECT_GE(3, change_count());
  ASSERT_EQ(3u, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0u);
  EXPECT_EQ(0u, item1.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 1"), item1.name);

  const AvatarMenu::Item& item2 = menu->GetItemAt(1u);
  EXPECT_EQ(1u, item2.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 2"), item2.name);

  const AvatarMenu::Item& item3 = menu->GetItemAt(2u);
  EXPECT_EQ(2u, item3.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 3"), item3.name);
}

}  // namespace

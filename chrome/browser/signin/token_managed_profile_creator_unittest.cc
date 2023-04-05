// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/token_managed_profile_creator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TokenManagedProfileCreatorTest : public testing::Test {
 public:
  TokenManagedProfileCreatorTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())) {}

  ~TokenManagedProfileCreatorTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_->SetUp()); }

  // Callback for the TokenManagedProfileCreator.
  void OnProfileCreated(base::OnceClosure quit_closure, Profile* profile) {
    creator_callback_called_ = true;
    created_profile_ = profile;
    if (quit_closure) {
      std::move(quit_closure).Run();
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> created_profile_;
  bool creator_callback_called_ = false;
};

TEST_F(TokenManagedProfileCreatorTest, CreatesProfileWithManagementInfo) {
  base::RunLoop loop;
  TokenManagedProfileCreator creator(
      "id", "enrollment_token", u"local_profile_name",
      base::BindOnce(&TokenManagedProfileCreatorTest::OnProfileCreated,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
  EXPECT_TRUE(creator_callback_called_);
  ASSERT_TRUE(created_profile_);

  auto* entry = TestingBrowserProcess::GetGlobal()
                    ->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(created_profile_->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("id", entry->GetProfileManagementId());
  EXPECT_EQ("enrollment_token", entry->GetProfileManagementEnrollmentToken());
  EXPECT_EQ(u"local_profile_name", entry->GetName());
}

TEST_F(TokenManagedProfileCreatorTest, LoadsExistingProfile) {
  auto* profile_manager = g_browser_process->profile_manager();
  Profile& new_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  base::FilePath path = new_profile.GetPath();
  {
    auto* entry = profile_manager->GetProfileAttributesStorage()
                      .GetProfileAttributesWithPath(path);
    entry->SetProfileManagementId("id");
    entry->SetProfileManagementEnrollmentToken("enrollment_token");
  }

  base::RunLoop loop;
  TokenManagedProfileCreator creator(
      path, base::BindOnce(&TokenManagedProfileCreatorTest::OnProfileCreated,
                           base::Unretained(this), loop.QuitClosure()));
  loop.Run();
  EXPECT_TRUE(creator_callback_called_);
  ASSERT_TRUE(created_profile_);
  EXPECT_EQ(path, created_profile_->GetPath());

  auto* entry = profile_manager->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(created_profile_->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("id", entry->GetProfileManagementId());
  EXPECT_EQ("enrollment_token", entry->GetProfileManagementEnrollmentToken());
}

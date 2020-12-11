// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_signed_in_profile_creator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kProfileTestName[] = "profile_test_name";

std::unique_ptr<TestingProfile> BuildTestingProfile(const base::FilePath& path,
                                                    Profile::Delegate* delegate,
                                                    bool tokens_loaded) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetDelegate(delegate);
  profile_builder.SetPath(path);
  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(profile_builder);
  if (!tokens_loaded) {
    IdentityTestEnvironmentProfileAdaptor adaptor(profile.get());
    adaptor.identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  }
  return profile;
}

// Simple ProfileManager creating testing profiles.
class UnittestProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ProfileManagerWithoutInit(user_data_dir) {}

  void set_tokens_loaded_at_creation(bool loaded) {
    tokens_loaded_at_creation_ = loaded;
  }

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    if (!base::PathExists(path) && !base::CreateDirectory(path))
      return nullptr;
    return BuildTestingProfile(path, /*delegate=*/nullptr,
                               tokens_loaded_at_creation_);
  }

  std::unique_ptr<Profile> CreateProfileAsyncHelper(
      const base::FilePath& path,
      Delegate* delegate) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));
    return BuildTestingProfile(path, this, tokens_loaded_at_creation_);
  }

  bool tokens_loaded_at_creation_ = true;
};

}  // namespace

class DiceSignedInProfileCreatorTest : public testing::Test,
                                       public ProfileManagerObserver {
 public:
  DiceSignedInProfileCreatorTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_manager_ = new UnittestProfileManager(temp_dir_.GetPath());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(profile_manager_);
    profile_ = BuildTestingProfile(base::FilePath(), /*delegate=*/nullptr,
                                   /*tokens_loaded=*/true);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    profile_manager()->AddObserver(this);
  }

  ~DiceSignedInProfileCreatorTest() override { DeleteProfiles(); }

  UnittestProfileManager* profile_manager() { return profile_manager_; }

  // Test environment attached to profile().
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Source profile (the one which we are extracting credentials from).
  Profile* profile() { return profile_.get(); }

  // Profile created by the DiceSignedInProfileCreator.
  Profile* signed_in_profile() { return signed_in_profile_; }

  // Profile added to the ProfileManager. In general this should be the same as
  // signed_in_profile() except in error cases.
  Profile* added_profile() { return added_profile_; }

  bool creator_callback_called() { return creator_callback_called_; }

  void set_profile_added_closure(base::OnceClosure closure) {
    profile_added_closure_ = std::move(closure);
  }

  void DeleteProfiles() {
    identity_test_env_profile_adaptor_.reset();
    if (profile_manager_) {
      profile_manager()->RemoveObserver(this);
      TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
      profile_manager_ = nullptr;
    }
  }

  // Callback for the DiceSignedInProfileCreator.
  void OnProfileCreated(base::OnceClosure quit_closure, Profile* profile) {
    creator_callback_called_ = true;
    signed_in_profile_ = profile;
    if (quit_closure)
      std::move(quit_closure).Run();
  }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    added_profile_ = profile;
    if (profile_added_closure_)
      std::move(profile_added_closure_).Run();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;
  UnittestProfileManager* profile_manager_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  Profile* signed_in_profile_ = nullptr;
  Profile* added_profile_ = nullptr;
  base::OnceClosure profile_added_closure_;
  bool creator_callback_called_ = false;
};

TEST_F(DiceSignedInProfileCreatorTest, CreateWithTokensLoaded) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  size_t kTestIcon = profiles::GetModernAvatarIconStartIndex();
  base::string16 kProfileTestName16 = base::UTF8ToUTF16(kProfileTestName);

  base::RunLoop loop;
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, kProfileTestName16, kTestIcon,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), loop.QuitClosure()));
  loop.Run();

  // Check that the account was moved.
  EXPECT_TRUE(creator_callback_called());
  EXPECT_TRUE(signed_in_profile());
  EXPECT_NE(profile(), signed_in_profile());
  EXPECT_EQ(signed_in_profile(), added_profile());
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(profile())
                   ->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_EQ(1u, IdentityManagerFactory::GetForProfile(signed_in_profile())
                    ->GetAccountsWithRefreshTokens()
                    .size());
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(signed_in_profile())
                  ->HasAccountWithRefreshToken(account_info.account_id));

  // Check the profile name and icon.
  ProfileAttributesEntry* entry = nullptr;
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ASSERT_TRUE(storage.GetProfileAttributesWithPath(
      signed_in_profile()->GetPath(), &entry));
  ASSERT_TRUE(entry);
  EXPECT_EQ(kProfileTestName16, entry->GetLocalProfileName());
  EXPECT_EQ(kTestIcon, entry->GetAvatarIconIndex());
}

TEST_F(DiceSignedInProfileCreatorTest, CreateWithTokensNotLoaded) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  profile_manager()->set_tokens_loaded_at_creation(false);

  base::RunLoop creator_loop;
  base::RunLoop profile_added_loop;
  set_profile_added_closure(profile_added_loop.QuitClosure());
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, base::string16(), base::nullopt,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), creator_loop.QuitClosure()));
  profile_added_loop.Run();
  base::RunLoop().RunUntilIdle();

  // The profile was created, but tokens not loaded. The callback has not been
  // called yet.
  EXPECT_FALSE(creator_callback_called());
  EXPECT_TRUE(added_profile());
  EXPECT_NE(profile(), added_profile());

  // Load the tokens.
  IdentityTestEnvironmentProfileAdaptor adaptor(added_profile());
  adaptor.identity_test_env()->ReloadAccountsFromDisk();
  creator_loop.Run();

  // Check that the account was moved.
  EXPECT_EQ(signed_in_profile(), added_profile());
  EXPECT_TRUE(creator_callback_called());
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(profile())
                   ->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_EQ(1u, IdentityManagerFactory::GetForProfile(signed_in_profile())
                    ->GetAccountsWithRefreshTokens()
                    .size());
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(signed_in_profile())
                  ->HasAccountWithRefreshToken(account_info.account_id));
}

// Deleting the creator while it is running does not crash.
TEST_F(DiceSignedInProfileCreatorTest, DeleteWhileCreating) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, base::string16(), base::nullopt,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), base::OnceClosure()));
  EXPECT_FALSE(creator_callback_called());
  creator.reset();
  base::RunLoop().RunUntilIdle();
}

// Deleting the profile while waiting for the tokens.
TEST_F(DiceSignedInProfileCreatorTest, DeleteProfile) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  profile_manager()->set_tokens_loaded_at_creation(false);

  base::RunLoop creator_loop;
  base::RunLoop profile_added_loop;
  set_profile_added_closure(profile_added_loop.QuitClosure());
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, base::string16(), base::nullopt,
          base::BindOnce(&DiceSignedInProfileCreatorTest::OnProfileCreated,
                         base::Unretained(this), creator_loop.QuitClosure()));
  profile_added_loop.Run();
  base::RunLoop().RunUntilIdle();

  // The profile was created, but tokens not loaded. The callback has not been
  // called yet.
  EXPECT_FALSE(creator_callback_called());
  EXPECT_TRUE(added_profile());
  EXPECT_NE(profile(), added_profile());

  DeleteProfiles();
  creator_loop.Run();

  // The callback is called with nullptr profile.
  EXPECT_TRUE(creator_callback_called());
  EXPECT_FALSE(signed_in_profile());
}

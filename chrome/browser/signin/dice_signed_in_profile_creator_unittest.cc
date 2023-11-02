// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_signed_in_profile_creator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char16_t kProfileTestName[] = u"profile_test_name";

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
  if (profile->GetPath() == ProfileManager::GetGuestProfilePath())
    profile->SetGuestSession(true);
  return profile;
}

class UnittestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : FakeProfileManager(user_data_dir) {}

  void set_tokens_loaded_at_creation(bool loaded) {
    tokens_loaded_at_creation_ = loaded;
  }

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Profile::Delegate* delegate) override {
    return ::BuildTestingProfile(path, delegate, tokens_loaded_at_creation_);
  }

  bool tokens_loaded_at_creation_ = true;
};

}  // namespace

class DiceSignedInProfileCreatorTest : public testing::Test,
                                       public ProfileManagerObserver {
 public:
  DiceSignedInProfileCreatorTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
    auto profile_manager_unique = std::make_unique<UnittestProfileManager>(
        base::CreateUniqueTempDirectoryScopedToTest());
    profile_manager_ = profile_manager_unique.get();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager_unique));
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

  bool use_guest_profile() const { return use_guest_profile_; }

  void DeleteProfiles() {
    identity_test_env_profile_adaptor_.reset();

    // Delete the profile first to make sure all observers to the profile
    // manager are cleared to avoid heap-use-after-free when the observer try to
    // stop observing the manager.
    profile_.reset();
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
  ScopedTestingLocalState local_state_;
  raw_ptr<UnittestProfileManager> profile_manager_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<Profile> signed_in_profile_ = nullptr;
  raw_ptr<Profile> added_profile_ = nullptr;
  base::OnceClosure profile_added_closure_;
  bool creator_callback_called_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
  bool use_guest_profile_ = false;
};

TEST_F(DiceSignedInProfileCreatorTest, CreateWithTokensLoaded) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  size_t kTestIcon = profiles::GetModernAvatarIconStartIndex();

  base::RunLoop loop;
  std::unique_ptr<DiceSignedInProfileCreator> creator =
      std::make_unique<DiceSignedInProfileCreator>(
          profile(), account_info.account_id, kProfileTestName, kTestIcon,
          use_guest_profile(),
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

  // Check profile type
  ASSERT_EQ(use_guest_profile(), signed_in_profile()->IsGuestSession());

  // Check the profile name and icon.
  ProfileAttributesStorage& storage =
      profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(signed_in_profile()->GetPath());
  ASSERT_TRUE(entry);
  if (!use_guest_profile()) {
    EXPECT_EQ(kProfileTestName, entry->GetLocalProfileName());
    EXPECT_EQ(kTestIcon, entry->GetAvatarIconIndex());
  }
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
          profile(), account_info.account_id, std::u16string(), absl::nullopt,
          use_guest_profile(),
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
          profile(), account_info.account_id, std::u16string(), absl::nullopt,
          use_guest_profile(),
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
          profile(), account_info.account_id, std::u16string(), absl::nullopt,
          use_guest_profile(),
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

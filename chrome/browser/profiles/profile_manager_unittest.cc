// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_supervision_transition.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "extensions/common/features/feature_session_type.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

using base::ASCIIToUTF16;

namespace {

// This global variable is used to check that value returned to different
// observers is the same.
Profile* g_created_profile = nullptr;

class UnittestProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ProfileManagerWithoutInit(user_data_dir) {}
  ~UnittestProfileManager() override = default;

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    if (!base::PathExists(path) && !base::CreateDirectory(path))
      return nullptr;
    return std::make_unique<TestingProfile>(path);
  }

  std::unique_ptr<Profile> CreateProfileAsyncHelper(
      const base::FilePath& path,
      Delegate* delegate) override {
    // ThreadTaskRunnerHandle::Get() is TestingProfile's "async" IOTaskRunner
    // (ref. TestingProfile::GetIOTaskRunner()).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));

    return std::make_unique<TestingProfile>(path, this);
  }
};

void ExpectNullProfile(base::Closure closure, Profile* profile) {
  EXPECT_EQ(nullptr, profile);
  closure.Run();
}

void ExpectProfileWithName(const std::string& profile_name,
                           bool incognito,
                           base::Closure closure,
                           Profile* profile) {
  EXPECT_NE(nullptr, profile);
  EXPECT_EQ(incognito, profile->IsOffTheRecord());
  if (incognito)
    profile = profile->GetOriginalProfile();

  // Create a profile on the fly so the the same comparison
  // can be used in Windows and other platforms.
  EXPECT_EQ(base::FilePath().AppendASCII(profile_name),
            profile->GetPath().BaseName());
  closure.Run();
}

}  // namespace

class ProfileManagerTest : public testing::Test {
 public:
  class MockObserver {
   public:
    MOCK_METHOD2(OnProfileCreated,
        void(Profile* profile, Profile::CreateStatus status));
  };

  ProfileManagerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  ~ProfileManagerTest() override = default;

  void SetUp() override {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        CreateProfileManagerForTest());

#if defined(OS_CHROMEOS)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClient>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    // Have to manually reset the session type in between test runs because
    // some tests log in users.
    ASSERT_EQ(extensions::FeatureSessionType::INITIAL,
              extensions::GetCurrentFeatureSessionType());
    session_type_ = extensions::ScopedCurrentFeatureSessionType(
        extensions::GetCurrentFeatureSessionType());
#endif
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    content::RunAllTasksUntilIdle();
#if defined(OS_CHROMEOS)
    session_type_.reset();
    wallpaper_controller_client_.reset();
#endif
  }

 protected:
  virtual ProfileManager* CreateProfileManagerForTest() {
    return new UnittestProfileManager(temp_dir_.GetPath());
  }

  // Helper function to create a profile with |name| for a profile |manager|.
  void CreateProfileAsync(ProfileManager* manager,
                          const std::string& name,
                          MockObserver* mock_observer) {
    manager->CreateProfileAsync(temp_dir_.GetPath().AppendASCII(name),
                                base::Bind(&MockObserver::OnProfileCreated,
                                           base::Unretained(mock_observer)),
                                base::UTF8ToUTF16(name),
                                profiles::GetDefaultAvatarIconUrl(0));
  }

  // Helper function to add a profile with |profile_name| to |profile_manager|'s
  // ProfileAttributesStorage, and return the profile created.
  Profile* AddProfileToStorage(ProfileManager* profile_manager,
                               const std::string& path_suffix,
                               const base::string16& profile_name) {
    ProfileAttributesStorage& storage =
        profile_manager->GetProfileAttributesStorage();
    size_t num_profiles = storage.GetNumberOfProfiles();
    base::FilePath path = temp_dir_.GetPath().AppendASCII(path_suffix);
    storage.AddProfile(path, profile_name, std::string(), base::string16(),
                       false, 0, std::string(), EmptyAccountId());
    EXPECT_EQ(num_profiles + 1u, storage.GetNumberOfProfiles());
    return profile_manager->GetProfile(path);
  }

  // Helper function to set profile ephemeral at prefs and attributes storage.
  void SetProfileEphemeral(Profile* profile) {
    profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

    // Update IsEphemeral in attributes storage, normally it happened via
    // kForceEphemeralProfiles pref change event routed to
    // ProfileImpl::UpdateIsEphemeralInStorage().
    ProfileAttributesEntry* entry;
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    EXPECT_TRUE(
        storage.GetProfileAttributesWithPath(profile->GetPath(), &entry));
    entry->SetIsEphemeral(true);
  }

#if defined(OS_CHROMEOS)
  // Helper function to register an user with id |user_id| and create profile
  // with a correct path.
  void RegisterUser(const AccountId& account_id) {
    chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
    const std::string user_id_hash =
        profile_helper->GetUserIdHashByUserIdForTesting(
            account_id.GetUserEmail());
    user_manager::UserManager::Get()->UserLoggedIn(account_id, user_id_hash,
                                                   false /* browser_restart */,
                                                   false /* is_child */);
    g_browser_process->profile_manager()->GetProfile(
        profile_helper->GetProfilePathByUserIdHash(user_id_hash));
  }

  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
#endif

  content::BrowserTaskEnvironment task_environment_;

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestUserManager test_user_manager_;
  std::unique_ptr<base::AutoReset<extensions::FeatureSessionType>>
      session_type_;
  std::unique_ptr<WallpaperControllerClient> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileManagerTest);
};

TEST_F(ProfileManagerTest, GetProfile) {
  base::FilePath dest_path = temp_dir_.GetPath();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create a profile.
  Profile* profile = profile_manager->GetProfile(dest_path);
  EXPECT_TRUE(profile);

  // The profile already exists when we call GetProfile. Just load it.
  EXPECT_EQ(profile, profile_manager->GetProfile(dest_path));
}

TEST_F(ProfileManagerTest, DefaultProfileDir) {
  base::FilePath expected_default =
      base::FilePath().AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(
      expected_default.value(),
      g_browser_process->profile_manager()->GetInitialProfileDir().value());
}

MATCHER(NotFail, "Profile creation failure status is not reported.") {
  return arg == Profile::CREATE_STATUS_CREATED ||
         arg == Profile::CREATE_STATUS_INITIALIZED;
}

MATCHER(SameNotNull, "The same non-NULL value for all calls.") {
  if (!g_created_profile)
    g_created_profile = arg;
  return arg && arg == g_created_profile;
}

#if defined(OS_CHROMEOS)

// This functionality only exists on Chrome OS.
TEST_F(ProfileManagerTest, LoggedInProfileDir) {
  base::FilePath expected_default =
      base::FilePath().AppendASCII(chrome::kInitialProfile);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(expected_default.value(),
            profile_manager->GetInitialProfileDir().value());

  constexpr char kTestUserName[] = "test-user@example.com";
  constexpr char kTestUserGaiaId[] = "0123456789";
  const AccountId test_account_id(
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId));
  chromeos::FakeChromeUserManager* user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager enabler(base::WrapUnique(user_manager));

  const user_manager::User* active_user =
      user_manager->AddUser(test_account_id);
  user_manager->LoginUser(test_account_id);
  user_manager->SwitchActiveUser(test_account_id);

  base::FilePath expected_logged_in(
      chromeos::ProfileHelper::GetUserProfileDir(active_user->username_hash()));
  EXPECT_EQ(expected_logged_in.value(),
            profile_manager->GetInitialProfileDir().value());
  VLOG(1) << temp_dir_.GetPath()
                 .Append(profile_manager->GetInitialProfileDir())
                 .value();
}

// Test Get[ActiveUser|PrimaryUser|LastUsed]Profile does not load user profile.
TEST_F(ProfileManagerTest, UserProfileLoading) {
  using chromeos::ProfileHelper;

  Profile* const signin_profile = ProfileHelper::GetSigninProfile();

  // Get[Active|Primary|LastUsed]Profile return the sign-in profile before login
  // happens. IsSameProfile() is used to properly test against TestProfile whose
  // OTR version uses a different temp path.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->IsSameProfile(signin_profile));
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->IsSameProfile(signin_profile));
  EXPECT_TRUE(
      ProfileManager::GetLastUsedProfile()->IsSameProfile(signin_profile));

  // User signs in but user profile loading has not started.
  const std::string user_id = "test-user@example.com";
  const std::string gaia_id = "0123456789";
  const std::string user_id_hash =
      ProfileHelper::Get()->GetUserIdHashByUserIdForTesting(user_id);
  user_manager::UserManager::Get()->UserLoggedIn(
      AccountId::FromUserEmailGaiaId(user_id, gaia_id), user_id_hash,
      false /* browser_restart */, false /* is_child */);

  // Sign-in profile should be returned at this stage. Otherwise, login code
  // ends up in an invalid state. Strange things as in http://crbug.com/728683
  // and http://crbug.com/718734 happens.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->IsSameProfile(signin_profile));
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->IsSameProfile(signin_profile));

  // GetLastUsedProfile() after login but before a user profile is loaded is
  // fatal.
  EXPECT_DEATH_IF_SUPPORTED(ProfileManager::GetLastUsedProfile(), ".*");

  // Simulate UserSessionManager loads the profile.
  Profile* const user_profile =
      g_browser_process->profile_manager()->GetProfile(
          ProfileHelper::Get()->GetProfilePathByUserIdHash(user_id_hash));
  ASSERT_FALSE(user_profile->IsSameProfile(signin_profile));

  // User profile is returned thereafter.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->IsSameProfile(user_profile));
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->IsSameProfile(user_profile));
  EXPECT_TRUE(
      ProfileManager::GetLastUsedProfile()->IsSameProfile(user_profile));
}

#endif  // defined(OS_CHROMEOS)

// Data race on Linux bots. http://crbug.com/789214
#if defined(OS_LINUX)
#define MAYBE_CreateAndUseTwoProfiles DISABLED_CreateAndUseTwoProfiles
#else
#define MAYBE_CreateAndUseTwoProfiles CreateAndUseTwoProfiles
#endif

TEST_F(ProfileManagerTest, MAYBE_CreateAndUseTwoProfiles) {
  base::FilePath dest_path1 = temp_dir_.GetPath();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  base::FilePath dest_path2 = temp_dir_.GetPath();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  // Force lazy-init of some profile services to simulate use.
  ASSERT_TRUE(profile1->CreateHistoryService(true, false));
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      profile1, ServiceAccessType::EXPLICIT_ACCESS));
  profile1->CreateBookmarkModel(true);
  EXPECT_TRUE(BookmarkModelFactory::GetForBrowserContext(profile1));
  profile2->CreateBookmarkModel(true);
  EXPECT_TRUE(BookmarkModelFactory::GetForBrowserContext(profile2));
  ASSERT_TRUE(profile2->CreateHistoryService(true, false));
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      profile2, ServiceAccessType::EXPLICIT_ACCESS));

  // Make sure any pending tasks run before we destroy the profiles.
  content::RunAllTasksUntilIdle();

  TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);

  // Make sure history cleans up correctly.
  content::RunAllTasksUntilIdle();
}

TEST_F(ProfileManagerTest, LoadNonExistingProfile) {
  const std::string profile_name = "NonExistingProfile";
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->LoadProfile(
      profile_name, false /* incognito */,
      base::Bind(&ExpectNullProfile, run_loop_1.QuitClosure()));
  run_loop_1.Run();

  profile_manager->LoadProfile(
      profile_name, true /* incognito */,
      base::Bind(&ExpectNullProfile, run_loop_2.QuitClosure()));
  run_loop_2.Run();
}

TEST_F(ProfileManagerTest, LoadExistingProfile) {
  const std::string profile_name = "MyProfile";
  const std::string other_name = "SomeOtherProfile";
  MockObserver mock_observer1;
  EXPECT_CALL(mock_observer1, OnProfileCreated(SameNotNull(), NotFail()))
      .Times(testing::AtLeast(1));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CreateProfileAsync(profile_manager, profile_name, &mock_observer1);

  // Make sure a real profile is created before continuing.
  content::RunAllTasksUntilIdle();

  base::RunLoop load_profile;
  bool incognito = false;
  profile_manager->LoadProfile(
      profile_name, incognito,
      base::Bind(&ExpectProfileWithName, profile_name, incognito,
                 load_profile.QuitClosure()));
  load_profile.Run();

  base::RunLoop load_profile_incognito;
  incognito = true;
  profile_manager->LoadProfile(
      profile_name, incognito,
      base::Bind(&ExpectProfileWithName, profile_name, incognito,
                 load_profile_incognito.QuitClosure()));
  load_profile_incognito.Run();

  // Loading some other non existing profile should still return null.
  base::RunLoop load_other_profile;
  profile_manager->LoadProfile(
      other_name, false,
      base::Bind(&ExpectNullProfile, load_other_profile.QuitClosure()));
  load_other_profile.Run();
}

TEST_F(ProfileManagerTest, CreateProfileAsyncMultipleRequests) {
  g_created_profile = nullptr;

  MockObserver mock_observer1;
  EXPECT_CALL(mock_observer1, OnProfileCreated(
      SameNotNull(), NotFail())).Times(testing::AtLeast(1));
  MockObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnProfileCreated(
      SameNotNull(), NotFail())).Times(testing::AtLeast(1));
  MockObserver mock_observer3;
  EXPECT_CALL(mock_observer3, OnProfileCreated(
      SameNotNull(), NotFail())).Times(testing::AtLeast(1));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const std::string profile_name = "New Profile";
  CreateProfileAsync(profile_manager, profile_name, &mock_observer1);
  CreateProfileAsync(profile_manager, profile_name, &mock_observer2);
  CreateProfileAsync(profile_manager, profile_name, &mock_observer3);

  content::RunAllTasksUntilIdle();
}

TEST_F(ProfileManagerTest, CreateProfilesAsync) {
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(3));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  CreateProfileAsync(profile_manager, profile_name1, &mock_observer);
  CreateProfileAsync(profile_manager, profile_name2, &mock_observer);

  content::RunAllTasksUntilIdle();
}

TEST_F(ProfileManagerTest, AddProfileToStorageCheckOmitted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  const base::FilePath supervised_path =
      temp_dir_.GetPath().AppendASCII("Supervised");
  auto supervised_profile =
      std::make_unique<TestingProfile>(supervised_path, nullptr);
  supervised_profile->GetPrefs()->SetString(
      prefs::kSupervisedUserId, supervised_users::kChildAccountSUID);

  // RegisterTestingProfile adds the profile to the cache and takes ownership.
  profile_manager->RegisterTestingProfile(std::move(supervised_profile), true,
                                          false);
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_TRUE(storage.GetAllProfilesAttributesSortedByName()[0]->IsOmitted());
#endif

  const base::FilePath nonsupervised_path =
      temp_dir_.GetPath().AppendASCII("Non-Supervised");
  auto nonsupervised_profile =
      std::make_unique<TestingProfile>(nonsupervised_path, nullptr);
  profile_manager->RegisterTestingProfile(std::move(nonsupervised_profile),
                                          true, false);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  EXPECT_EQ(2u, storage.GetNumberOfProfiles());
#else
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());
#endif
  ProfileAttributesEntry* entry;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  ASSERT_TRUE(storage.GetProfileAttributesWithPath(supervised_path, &entry));
  EXPECT_TRUE(entry->IsOmitted());
#endif

  ASSERT_TRUE(storage.GetProfileAttributesWithPath(nonsupervised_path, &entry));
  EXPECT_FALSE(entry->IsOmitted());
}

TEST_F(ProfileManagerTest, GetGuestProfilePath) {
  base::FilePath guest_path = ProfileManager::GetGuestProfilePath();
  base::FilePath expected_path = temp_dir_.GetPath();
  expected_path = expected_path.Append(chrome::kGuestProfileDir);
  EXPECT_EQ(expected_path, guest_path);
}

TEST_F(ProfileManagerTest, GetSystemProfilePath) {
  base::FilePath system_profile_path = ProfileManager::GetSystemProfilePath();
  base::FilePath expected_path = temp_dir_.GetPath();
  expected_path = expected_path.Append(chrome::kSystemProfileDir);
  EXPECT_EQ(expected_path, system_profile_path);
}

class UnittestGuestProfileManager : public UnittestProfileManager {
 public:
  explicit UnittestGuestProfileManager(const base::FilePath& user_data_dir)
      : UnittestProfileManager(user_data_dir) {}

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    TestingProfile::Builder builder;
    builder.SetGuestSession();
    builder.SetPath(path);
    return builder.Build();
  }
};

class ProfileManagerGuestTest : public ProfileManagerTest  {
 public:
  ProfileManagerGuestTest() = default;
  ~ProfileManagerGuestTest() override = default;

  void SetUp() override {
#if defined(OS_CHROMEOS)
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitch(chromeos::switches::kGuestSession);
    cl->AppendSwitch(::switches::kIncognito);
#endif

    ProfileManagerTest::SetUp();

#if defined(OS_CHROMEOS)
    RegisterUser(GetFakeUserManager()->GetGuestAccountId());
#endif
  }

 protected:
  ProfileManager* CreateProfileManagerForTest() override {
    return new UnittestGuestProfileManager(temp_dir_.GetPath());
  }

#if defined(OS_CHROMEOS)
  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileManagerGuestTest);
};

TEST_F(ProfileManagerGuestTest, GetLastUsedProfileAllowedByPolicy) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  Profile* profile = profile_manager->GetLastUsedProfileAllowedByPolicy();
  ASSERT_TRUE(profile);
  EXPECT_TRUE(profile->IsOffTheRecord());
}

#if defined(OS_CHROMEOS)
TEST_F(ProfileManagerGuestTest, GuestProfileIngonito) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  EXPECT_TRUE(primary_profile->IsOffTheRecord());

  Profile* active_profile = ProfileManager::GetActiveUserProfile();
  EXPECT_TRUE(active_profile->IsOffTheRecord());

  EXPECT_TRUE(active_profile->IsSameProfile(primary_profile));

  Profile* last_used_profile = ProfileManager::GetLastUsedProfile();
  EXPECT_TRUE(last_used_profile->IsOffTheRecord());

  EXPECT_TRUE(last_used_profile->IsSameProfile(active_profile));
}
#endif

TEST_F(ProfileManagerTest, AutoloadProfilesWithBackgroundApps) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  local_state_.Get()->SetUserPref(prefs::kBackgroundModeEnabled,
                                  std::make_unique<base::Value>(true));

  // Setting a pref which is not applicable to a system (i.e., Android in this
  // case) does not necessarily create it. Don't bother continuing with the
  // test if this pref doesn't exist because it will not load the profiles if
  // it cannot verify that the pref for background mode is enabled.
  if (!local_state_.Get()->HasPrefPath(prefs::kBackgroundModeEnabled))
    return;

  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  storage.AddProfile(profile_manager->user_data_dir().AppendASCII("path_1"),
                     ASCIIToUTF16("name_1"), "12345", base::string16(), true, 0,
                     std::string(), EmptyAccountId());
  storage.AddProfile(profile_manager->user_data_dir().AppendASCII("path_2"),
                     ASCIIToUTF16("name_2"), "23456", base::string16(), true, 0,
                     std::string(), EmptyAccountId());
  storage.AddProfile(profile_manager->user_data_dir().AppendASCII("path_3"),
                     ASCIIToUTF16("name_3"), "34567", base::string16(), false,
                     0, std::string(), EmptyAccountId());

  ASSERT_EQ(3u, storage.GetNumberOfProfiles());

  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  entries[0]->SetBackgroundStatus(true);
  entries[2]->SetBackgroundStatus(true);

  profile_manager->AutoloadProfiles();

  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
}

TEST_F(ProfileManagerTest, DoNotAutoloadProfilesIfBackgroundModeOff) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  local_state_.Get()->SetUserPref(prefs::kBackgroundModeEnabled,
                                  std::make_unique<base::Value>(false));

  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  storage.AddProfile(profile_manager->user_data_dir().AppendASCII("path_1"),
                     ASCIIToUTF16("name_1"), "12345", base::string16(), true, 0,
                     std::string(), EmptyAccountId());
  storage.AddProfile(profile_manager->user_data_dir().AppendASCII("path_2"),
                     ASCIIToUTF16("name_2"), "23456", base::string16(), true, 0,
                     std::string(), EmptyAccountId());

  ASSERT_EQ(2u, storage.GetNumberOfProfiles());

  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  entries[0]->SetBackgroundStatus(false);
  entries[1]->SetBackgroundStatus(true);

  profile_manager->AutoloadProfiles();

  EXPECT_EQ(0u, profile_manager->GetLoadedProfiles().size());
}

TEST_F(ProfileManagerTest, InitProfileUserPrefs) {
  base::FilePath dest_path = temp_dir_.GetPath();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  Profile* profile;

  // Successfully create the profile
  profile = profile_manager->GetProfile(dest_path);
  ASSERT_TRUE(profile);

  // Check that the profile name is non empty
  std::string profile_name =
      profile->GetPrefs()->GetString(prefs::kProfileName);
  EXPECT_FALSE(profile_name.empty());

  // Check that the profile avatar index is valid
  size_t avatar_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);
  EXPECT_TRUE(profiles::IsDefaultAvatarIconIndex(
      avatar_index));
}

// Tests that a new profile's entry in the profile info cache is setup with the
// same values that are in the profile prefs.
TEST_F(ProfileManagerTest, InitProfileInfoCacheForAProfile) {
  base::FilePath dest_path = temp_dir_.GetPath();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profile
  Profile* profile = profile_manager->GetProfile(dest_path);
  ASSERT_TRUE(profile);

  std::string profile_name =
      profile->GetPrefs()->GetString(prefs::kProfileName);
  size_t avatar_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager->GetProfileAttributesStorage().
                  GetProfileAttributesWithPath(dest_path, &entry));

  // Check if the profile prefs are the same as the cache prefs
  EXPECT_EQ(profile_name, base::UTF16ToUTF8(entry->GetName()));
  EXPECT_EQ(avatar_index, entry->GetAvatarIconIndex());
}

#if defined(OS_CHROMEOS)
TEST_F(ProfileManagerTest, InitProfileForChildOnFirstSignIn) {
  chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const std::string user_email = "child@example.com";
  const AccountId account_id = AccountId::FromUserEmailGaiaId(user_email, "1");
  const std::string user_id_hash =
      profile_helper->GetUserIdHashByUserIdForTesting(user_email);
  const base::FilePath dest_path =
      profile_helper->GetProfilePathByUserIdHash(user_id_hash);

  TestingProfile::Builder builder;
  builder.SetPath(dest_path);
  builder.OverrideIsNewProfile(true);
  std::unique_ptr<Profile> profile = builder.Build();

  user_manager->UserLoggedIn(account_id, user_id_hash,
                             false /* browser_restart */, true /* is_child */);
  g_browser_process->profile_manager()->InitProfileUserPrefs(profile.get());

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcSupervisionTransition),
      static_cast<int>(arc::ArcSupervisionTransition::NO_TRANSITION));
  EXPECT_EQ(profile->GetPrefs()->GetString(prefs::kSupervisedUserId),
            supervised_users::kChildAccountSUID);
}

TEST_F(ProfileManagerTest, InitProfileForRegularToChildTransition) {
  chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const std::string user_email = "child@example.com";
  const AccountId account_id = AccountId::FromUserEmailGaiaId(user_email, "1");
  const std::string user_id_hash =
      profile_helper->GetUserIdHashByUserIdForTesting(user_email);
  const base::FilePath dest_path =
      profile_helper->GetProfilePathByUserIdHash(user_id_hash);

  TestingProfile::Builder builder;
  builder.SetPath(dest_path);
  builder.OverrideIsNewProfile(false);
  std::unique_ptr<Profile> profile = builder.Build();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);

  user_manager->UserLoggedIn(account_id, user_id_hash,
                             false /* browser_restart */, true /* is_child */);
  g_browser_process->profile_manager()->InitProfileUserPrefs(profile.get());

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcSupervisionTransition),
      static_cast<int>(arc::ArcSupervisionTransition::REGULAR_TO_CHILD));
  EXPECT_EQ(profile->GetPrefs()->GetString(prefs::kSupervisedUserId),
            supervised_users::kChildAccountSUID);
}

TEST_F(ProfileManagerTest, InitProfileForChildToRegularTransition) {
  chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const std::string user_email = "child@example.com";
  const AccountId account_id = AccountId::FromUserEmailGaiaId(user_email, "1");
  const std::string user_id_hash =
      profile_helper->GetUserIdHashByUserIdForTesting(user_email);
  const base::FilePath dest_path =
      profile_helper->GetProfilePathByUserIdHash(user_id_hash);

  TestingProfile::Builder builder;
  builder.SetPath(dest_path);
  builder.OverrideIsNewProfile(false);
  builder.SetSupervisedUserId(supervised_users::kChildAccountSUID);
  std::unique_ptr<Profile> profile = builder.Build();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);

  user_manager->UserLoggedIn(account_id, user_id_hash,
                             false /* browser_restart */, false /* is_child */);
  g_browser_process->profile_manager()->InitProfileUserPrefs(profile.get());

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcSupervisionTransition),
      static_cast<int>(arc::ArcSupervisionTransition::CHILD_TO_REGULAR));
  EXPECT_TRUE(profile->GetPrefs()->GetString(prefs::kSupervisedUserId).empty());
}

TEST_F(ProfileManagerTest,
       InitProfileForChildToRegularTransitionArcNotSignedIn) {
  chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const std::string user_email = "child@example.com";
  const AccountId account_id = AccountId::FromUserEmailGaiaId(user_email, "1");
  const std::string user_id_hash =
      profile_helper->GetUserIdHashByUserIdForTesting(user_email);
  const base::FilePath dest_path =
      profile_helper->GetProfilePathByUserIdHash(user_id_hash);

  TestingProfile::Builder builder;
  builder.SetPath(dest_path);
  builder.OverrideIsNewProfile(false);
  builder.SetSupervisedUserId(supervised_users::kChildAccountSUID);
  std::unique_ptr<Profile> profile = builder.Build();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, false);

  user_manager->UserLoggedIn(account_id, user_id_hash,
                             false /* browser_restart */, false /* is_child */);
  g_browser_process->profile_manager()->InitProfileUserPrefs(profile.get());

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcSupervisionTransition),
      static_cast<int>(arc::ArcSupervisionTransition::NO_TRANSITION));
  EXPECT_TRUE(profile->GetPrefs()->GetString(prefs::kSupervisedUserId).empty());
}

#endif

TEST_F(ProfileManagerTest, GetLastUsedProfileAllowedByPolicy) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

#if defined(OS_CHROMEOS)
  // On CrOS, profile returned by GetLastUsedProfile is a sign-in profile that
  // is forced to be incognito. That's why we need to create at least one user
  // to get a regular profile.
  RegisterUser(
      AccountId::FromUserEmailGaiaId("test-user@example.com", "1234567890"));
#endif

  Profile* profile = profile_manager->GetLastUsedProfileAllowedByPolicy();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->IsOffTheRecord());
  PrefService* prefs = profile->GetPrefs();
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability,
            IncognitoModePrefs::GetAvailability(prefs));

  ASSERT_TRUE(profile->GetOffTheRecordProfile());

  IncognitoModePrefs::SetAvailability(prefs, IncognitoModePrefs::DISABLED);
  EXPECT_FALSE(
      profile_manager->GetLastUsedProfileAllowedByPolicy()->IsOffTheRecord());

  // GetLastUsedProfileAllowedByPolicy() returns the incognito Profile when
  // incognito mode is forced.
  IncognitoModePrefs::SetAvailability(prefs, IncognitoModePrefs::FORCED);
  EXPECT_TRUE(
      profile_manager->GetLastUsedProfileAllowedByPolicy()->IsOffTheRecord());
}

#if !defined(OS_ANDROID)
// There's no Browser object on Android.
TEST_F(ProfileManagerTest, LastOpenedProfiles) {
  base::FilePath dest_path1 = temp_dir_.GetPath();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  base::FilePath dest_path2 = temp_dir_.GetPath();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(profile1, true);
  std::unique_ptr<Browser> browser1a(
      CreateBrowserWithTestWindowForParams(&profile1_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  Browser::CreateParams profile2_params(profile2, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(&profile2_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Adding more browsers doesn't change anything.
  std::unique_ptr<Browser> browser1b(
      CreateBrowserWithTestWindowForParams(&profile1_params));
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Close the browsers.
  browser1a.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  browser1b.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile2, last_opened_profiles[0]);

  browser2.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());
}

TEST_F(ProfileManagerTest, LastOpenedProfilesAtShutdown) {
  base::FilePath dest_path1 = temp_dir_.GetPath();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  base::FilePath dest_path2 = temp_dir_.GetPath();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(profile1, true);
  std::unique_ptr<Browser> browser1(
      CreateBrowserWithTestWindowForParams(&profile1_params));

  // And for profile2.
  Browser::CreateParams profile2_params(profile2, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(&profile2_params));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Simulate a shutdown.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  // Even if the browsers are destructed during shutdown, the profiles stay
  // open.
  browser1.reset();
  browser2.reset();

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);
}

TEST_F(ProfileManagerTest, LastOpenedProfilesDoesNotContainIncognito) {
  base::FilePath dest_path1 = temp_dir_.GetPath();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = temp_dir_.GetPath();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(profile1, true);
  std::unique_ptr<Browser> browser1(
      CreateBrowserWithTestWindowForParams(&profile1_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  Browser::CreateParams profile2_params(profile1->GetOffTheRecordProfile(),
                                        true);
  std::unique_ptr<Browser> browser2a(
      CreateBrowserWithTestWindowForParams(&profile2_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // Adding more browsers doesn't change anything.
  std::unique_ptr<Browser> browser2b(
      CreateBrowserWithTestWindowForParams(&profile2_params));
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // Close the browsers.
  browser2a.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  browser2b.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  browser1.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
// There's no Browser object on Android and there's no multi-profiles on Chrome.
TEST_F(ProfileManagerTest, EphemeralProfilesDontEndUpAsLastProfile) {
  base::FilePath dest_path = temp_dir_.GetPath();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("Ephemeral Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  TestingProfile* profile =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path));
  ASSERT_TRUE(profile);
  SetProfileEphemeral(profile);

  // Here the last used profile is still the "Default" profile.
  Profile* last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile, last_used_profile);

  // Create a browser for the profile.
  Browser::CreateParams profile_params(profile, true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(&profile_params));
  last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile, last_used_profile);

  // Close the browser.
  browser.reset();
  last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile, last_used_profile);
}

TEST_F(ProfileManagerTest, EphemeralProfilesDontEndUpAsLastOpenedAtShutdown) {
  base::FilePath dest_path1 = temp_dir_.GetPath();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("Normal Profile"));

  base::FilePath dest_path2 = temp_dir_.GetPath();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("Ephemeral Profile 1"));

  base::FilePath dest_path3 = temp_dir_.GetPath();
  dest_path3 = dest_path3.Append(FILE_PATH_LITERAL("Ephemeral Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* normal_profile =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(normal_profile);

  // Add one ephemeral profile which should not end up in this list.
  TestingProfile* ephemeral_profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(ephemeral_profile1);
  SetProfileEphemeral(ephemeral_profile1);

  // Add second ephemeral profile but don't mark it as such yet.
  TestingProfile* ephemeral_profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path3));
  ASSERT_TRUE(ephemeral_profile2);

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(normal_profile, true);
  std::unique_ptr<Browser> browser1(
      CreateBrowserWithTestWindowForParams(&profile1_params));

  // Create browsers for the ephemeral profile.
  Browser::CreateParams profile2_params(ephemeral_profile1, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(&profile2_params));

  Browser::CreateParams profile3_params(ephemeral_profile2, true);
  std::unique_ptr<Browser> browser3(
      CreateBrowserWithTestWindowForParams(&profile3_params));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(normal_profile, last_opened_profiles[0]);
  EXPECT_EQ(ephemeral_profile2, last_opened_profiles[1]);

  // Mark the second profile ephemeral.
  SetProfileEphemeral(ephemeral_profile2);

  // Simulate a shutdown.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  browser1.reset();
  browser2.reset();
  browser3.reset();

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(normal_profile, last_opened_profiles[0]);
}

TEST_F(ProfileManagerTest, CleanUpEphemeralProfiles) {
  // Create two profiles, one of them ephemeral.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ASSERT_EQ(0u, storage.GetNumberOfProfiles());

  const std::string profile_name1 = "Homer";
  base::FilePath path1 =
      profile_manager->user_data_dir().AppendASCII(profile_name1);
  storage.AddProfile(path1, base::UTF8ToUTF16(profile_name1), std::string(),
                     base::UTF8ToUTF16(profile_name1), true, 0, std::string(),
                     EmptyAccountId());
  storage.GetAllProfilesAttributes()[0]->SetIsEphemeral(true);
  ASSERT_TRUE(base::CreateDirectory(path1));

  const std::string profile_name2 = "Marge";
  base::FilePath path2 =
      profile_manager->user_data_dir().AppendASCII(profile_name2);
  storage.AddProfile(path2, base::UTF8ToUTF16(profile_name2), std::string(),
                     base::UTF8ToUTF16(profile_name2), true, 0, std::string(),
                     EmptyAccountId());
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());
  ASSERT_TRUE(base::CreateDirectory(path2));

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_name1);

  profile_manager->CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();

  // The ephemeral profile should be deleted, and the last used profile set to
  // the other one.
  EXPECT_FALSE(base::DirectoryExists(path1));
  EXPECT_TRUE(base::DirectoryExists(path2));
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // Mark the remaining profile ephemeral and clean up.
  storage.GetAllProfilesAttributes()[0]->SetIsEphemeral(true);
  profile_manager->CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();

  // The profile should be deleted, and the last used profile set to a new one.
  EXPECT_FALSE(base::DirectoryExists(path2));
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());
  EXPECT_EQ("Profile 1", local_state->GetString(prefs::kProfileLastUsed));
}

TEST_F(ProfileManagerTest, CleanUpEphemeralProfilesWithGuestLastUsedProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ASSERT_EQ(0u, storage.GetNumberOfProfiles());

  const std::string profile_name1 = "Homer";
  base::FilePath path1 =
      profile_manager->user_data_dir().AppendASCII(profile_name1);
  storage.AddProfile(path1, base::UTF8ToUTF16(profile_name1), std::string(),
                     base::UTF8ToUTF16(profile_name1), true, 0, std::string(),
                     EmptyAccountId());
  storage.GetAllProfilesAttributes()[0]->SetIsEphemeral(true);
  ASSERT_TRUE(base::CreateDirectory(path1));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, std::string("Guest Profile"));

  profile_manager->CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(0u, storage.GetNumberOfProfiles());
  EXPECT_EQ("Profile 1", local_state->GetString(prefs::kProfileLastUsed));
}

TEST_F(ProfileManagerTest, ActiveProfileDeleted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load two profiles.
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";
  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII(profile_name1);
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII(profile_name2);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(3));

  CreateProfileAsync(profile_manager, profile_name1, &mock_observer);
  CreateProfileAsync(profile_manager, profile_name2, &mock_observer);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(2u, profile_manager->GetProfileAttributesStorage().
                    GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_name1);

  // Delete the active profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1, base::DoNothing());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(dest_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
}

TEST_F(ProfileManagerTest, LastProfileDeleted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create and load a profile.
  const std::string profile_name1 = "New Profile 1";
  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII(profile_name1);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(1));

  CreateProfileAsync(profile_manager, profile_name1, &mock_observer);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Set it as the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_name1);

  // Delete the active profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1, base::DoNothing());
  content::RunAllTasksUntilIdle();

  // A new profile should have been created
  const std::string profile_name2 = "Profile 1";
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII(profile_name2);

  EXPECT_EQ(dest_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(dest_path2, storage.GetAllProfilesAttributes()[0]->GetPath());
}

TEST_F(ProfileManagerTest, LastProfileDeletedWithGuestActiveProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create and load a profile.
  const std::string profile_name1 = "New Profile 1";
  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII(profile_name1);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));

  CreateProfileAsync(profile_manager, profile_name1, &mock_observer);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Create the profile and register it.
  const std::string guest_profile_name =
      ProfileManager::GetGuestProfilePath().BaseName().MaybeAsASCII();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());
  std::unique_ptr<TestingProfile> guest_profile = builder.Build();
  guest_profile->set_profile_name(guest_profile_name);
  profile_manager->RegisterTestingProfile(std::move(guest_profile), false,
                                          false);

  // The Guest profile does not get added to the ProfileAttributesStorage.
  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Set the Guest profile as the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, guest_profile_name);

  // Delete the other profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1, base::DoNothing());
  content::RunAllTasksUntilIdle();

  // A new profile should have been created.
  const std::string profile_name2 = "Profile 1";
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII(profile_name2);

  EXPECT_EQ(3u, profile_manager->GetLoadedProfiles().size());
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(dest_path2, storage.GetAllProfilesAttributes()[0]->GetPath());
}

TEST_F(ProfileManagerTest, ProfileDisplayNameResetsDefaultName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const base::string16 default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const base::string16 profile_name1 = storage.ChooseNameForNewProfile(0u);
  Profile* profile1 = AddProfileToStorage(profile_manager,
                                          "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const base::string16 profile_name2 = storage.ChooseNameForNewProfile(1u);
  Profile* profile2 = AddProfileToStorage(profile_manager,
                                          "path_2", profile_name2);
  EXPECT_EQ(profile_name1,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the default name.
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              base::DoNothing());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}

TEST_F(ProfileManagerTest, ProfileDisplayNamePreservesCustomName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const base::string16 default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const base::string16 profile_name1 = storage.ChooseNameForNewProfile(0u);
  Profile* profile1 = AddProfileToStorage(profile_manager,
                                          "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // We should display custom names for local profiles.
  const base::string16 custom_profile_name = ASCIIToUTF16("Batman");
  ProfileAttributesEntry* entry = storage.GetAllProfilesAttributes()[0];
  entry->SetLocalProfileName(custom_profile_name);
  entry->SetIsUsingDefaultName(false);
  EXPECT_EQ(custom_profile_name, entry->GetName());
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const base::string16 profile_name2 = storage.ChooseNameForNewProfile(1u);
  Profile* profile2 = AddProfileToStorage(profile_manager,
                                          "path_2", profile_name2);
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the original, custom name.
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              base::DoNothing());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}

TEST_F(ProfileManagerTest, ProfileDisplayNamePreservesSignedInName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const base::string16 default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const base::string16 profile_name1 = storage.ChooseNameForNewProfile(0u);
  Profile* profile1 = AddProfileToStorage(profile_manager,
                                          "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage.GetAllProfilesAttributes()[0];
  // For a signed in profile with a default name we still display
  // IDS_SINGLE_PROFILE_DISPLAY_NAME.
  entry->SetAuthInfo("12345", ASCIIToUTF16("user@gmail.com"), true);
  EXPECT_EQ(profile_name1, entry->GetName());
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // For a signed in profile with a non-default Gaia given name we display the
  // Gaia given name.
  entry->SetAuthInfo("12345", ASCIIToUTF16("user@gmail.com"), true);
  const base::string16 gaia_given_name(ASCIIToUTF16("given name"));
  entry->SetGAIAGivenName(gaia_given_name);
  EXPECT_EQ(gaia_given_name, entry->GetName());
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const base::string16 profile_name2 = storage.ChooseNameForNewProfile(1u);
  Profile* profile2 = AddProfileToStorage(profile_manager,
                                          "path_2", profile_name2);
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the original, actual profile name.
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              base::DoNothing());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}

TEST_F(ProfileManagerTest, ProfileDisplayNameIsEmailIfDefaultName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // This test is relevant on desktop if |kProfileMenuRevamp| is disabled. If
  // it is enabled for pre-existing directory with legacy profile name, they
  // will be migrated to new default profile name |Person %n|. For newly created
  // profiles, only |Person %n| is considered as a default profile name.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kProfileMenuRevamp);
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  // Create two signed in profiles, with both new and legacy default names, and
  // a profile with a custom name.
  Profile* profile1 = AddProfileToStorage(profile_manager, "path_1",
                                          ASCIIToUTF16("Person 1"));
  Profile* profile2 = AddProfileToStorage(profile_manager, "path_2",
                                          ASCIIToUTF16("Default Profile"));
  const base::string16 profile_name3(ASCIIToUTF16("Batman"));
  Profile* profile3 = AddProfileToStorage(profile_manager, "path_3",
                                          profile_name3);
  EXPECT_EQ(3u, storage.GetNumberOfProfiles());

  // Sign in all profiles, and make sure they do not have a Gaia name set.
  const base::string16 email1(ASCIIToUTF16("user1@gmail.com"));
  const base::string16 email2(ASCIIToUTF16("user2@gmail.com"));
  const base::string16 email3(ASCIIToUTF16("user3@gmail.com"));

  ProfileAttributesEntry* entry;

  ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile1->GetPath(),
                                                   &entry));
  entry->SetAuthInfo("12345", email1, true);
  entry->SetGAIAGivenName(base::string16());
  entry->SetGAIAName(base::string16());

  ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile2->GetPath(),
                                                   &entry));
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // (Default profile, Batman,..) are legacy profile names on Desktop and are
  // not considered default profile names for newly created profiles.
  // We use "Person %n" as the default profile name. Set |SetIsUsingDefaultName|
  // manually to mimick pre-existing profiles.
  entry->SetIsUsingDefaultName(true);
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

  entry->SetAuthInfo("23456", email2, true);
  entry->SetGAIAGivenName(base::string16());
  entry->SetGAIAName(base::string16());

  ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile3->GetPath(),
                                                   &entry));

  entry->SetAuthInfo("34567", email3, true);
  entry->SetGAIAGivenName(base::string16());
  entry->SetGAIAName(base::string16());

  // The profiles with default names should display the email address.
  EXPECT_EQ(email1, profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(email2, profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // The profile with the custom name should display that.
  EXPECT_EQ(profile_name3,
            profiles::GetAvatarNameForProfile(profile3->GetPath()));

  // Adding a Gaia name to a profile that previously had a default name should
  // start displaying it.
  const base::string16 gaia_given_name(ASCIIToUTF16("Robin"));
  ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile1->GetPath(),
                                                   &entry));
  entry->SetGAIAGivenName(gaia_given_name);
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// These tests are for a Mac-only code path that assumes the browser
// process isn't killed when all browser windows are closed.
TEST_F(ProfileManagerTest, ActiveProfileDeletedNeedsToLoadNextProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load one profile, and just create a second profile.
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";
  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII(profile_name1);
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII(profile_name2);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));
  CreateProfileAsync(profile_manager, profile_name1, &mock_observer);
  content::RunAllTasksUntilIdle();

  // Track the profile, but don't load it.
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  storage.AddProfile(dest_path2, ASCIIToUTF16(profile_name2), "23456",
                     base::string16(), true, 0, std::string(),
                     EmptyAccountId());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(2u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  // Delete the active profile. This should switch and load the unloaded
  // profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1, base::DoNothing());

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(dest_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
}

// This tests the recursive call in ProfileManager::OnNewActiveProfileLoaded
// by simulating a scenario in which the profile that is being loaded as
// the next active profile has also been marked for deletion, so the
// ProfileManager needs to recursively select a different next profile.
TEST_F(ProfileManagerTest, ActiveProfileDeletedNextProfileDeletedToo) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load one profile, and create two more profiles.
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";
  const std::string profile_name3 = "New Profile 3";
  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII(profile_name1);
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII(profile_name2);
  base::FilePath dest_path3 = temp_dir_.GetPath().AppendASCII(profile_name3);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));
  CreateProfileAsync(profile_manager, profile_name1, &mock_observer);
  content::RunAllTasksUntilIdle();

  // Create the other profiles, but don't load them. Assign a fake avatar icon
  // to ensure that profiles in the profile attributes storage are sorted by the
  // profile name, and not randomly by the avatar name.
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  storage.AddProfile(dest_path2, ASCIIToUTF16(profile_name2), "23456",
                     ASCIIToUTF16(profile_name2), true, 1, std::string(),
                     EmptyAccountId());
  storage.AddProfile(dest_path3, ASCIIToUTF16(profile_name3), "34567",
                     ASCIIToUTF16(profile_name3), true, 2, std::string(),
                     EmptyAccountId());

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(3u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  // Delete the active profile, Profile1.
  // This will post a CreateProfileAsync message, that tries to load Profile2,
  // which checks that the profile is not being deleted, and then calls back
  // FinishDeletingProfile for Profile1.
  // Try to break this flow by setting the active profile to Profile2 in the
  // middle (so after the first posted message), and trying to delete Profile2,
  // so that the ProfileManager has to look for a different profile to load.
  profile_manager->ScheduleProfileForDeletion(dest_path1, base::DoNothing());
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path2.BaseName().MaybeAsASCII());
  profile_manager->ScheduleProfileForDeletion(dest_path2, base::DoNothing());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(dest_path3, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name3, local_state->GetString(prefs::kProfileLastUsed));
}
#endif  // defined(OS_MACOSX)

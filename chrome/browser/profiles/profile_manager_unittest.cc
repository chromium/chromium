// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "components/user_manager/user_names.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using base::ASCIIToUTF16;

namespace {

// This global variable is used to check that value returned to different
// observers is the same.
Profile* g_created_profile = nullptr;

void ExpectNullProfile(base::OnceClosure closure, Profile* profile) {
  EXPECT_EQ(nullptr, profile);
  std::move(closure).Run();
}

void ExpectProfileWithName(const std::string& profile_name,
                           bool incognito,
                           base::OnceClosure closure,
                           Profile* profile) {
  EXPECT_NE(nullptr, profile);
  EXPECT_EQ(incognito, profile->IsOffTheRecord());
  if (incognito)
    profile = profile->GetOriginalProfile();

  // Create a profile on the fly so the the same comparison
  // can be used in Windows and other platforms.
  EXPECT_EQ(base::FilePath().AppendASCII(profile_name), profile->GetBaseName());
  std::move(closure).Run();
}

class ProfileDeletionWaiter {
 public:
  explicit ProfileDeletionWaiter(const Profile* profile)
      : profile_path_(profile->GetPath()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ProfileDeletionWaiter(const ProfileDeletionWaiter&) = delete;
  ProfileDeletionWaiter& operator=(const ProfileDeletionWaiter&) = delete;

  void Wait() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!watcher_);
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ProfileDeletionWaiter::StartWatchingPath,
                                  base::Unretained(this)));
    run_loop_.Run();
  }

 private:
  void StartWatchingPath() {
    DCHECK(!watcher_);
    watcher_ = std::make_unique<base::FilePathWatcher>();
    EXPECT_TRUE(watcher_->Watch(
        profile_path_, base::FilePathWatcher::Type::kNonRecursive,
        base::BindRepeating(&ProfileDeletionWaiter::OnChanged,
                            base::Unretained(this))));
    CheckIfPathExists();
  }

  void OnChanged(const base::FilePath& path, bool error) {
    EXPECT_EQ(profile_path_, path);
    EXPECT_FALSE(error);
    CheckIfPathExists();
  }

  void CheckIfPathExists() {
    if (!base::PathExists(profile_path_)) {
      watcher_.reset();
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  const base::FilePath profile_path_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
};

}  // namespace

class ProfileManagerTest : public testing::Test {
 public:
  class MockObserver {
   public:
    MOCK_METHOD1(OnProfileInitialized, void(Profile* profile));
    MOCK_METHOD1(OnProfileCreated, void(Profile* profile));
  };

  ProfileManagerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  ProfileManagerTest(const ProfileManagerTest&) = delete;
  ProfileManagerTest& operator=(const ProfileManagerTest&) = delete;
  ~ProfileManagerTest() override = default;

  void SetUp() override {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        CreateProfileManagerForTest());

#if BUILDFLAG(IS_CHROMEOS)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
    ash::UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();
    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    // Have to manually reset the session type in between test runs because
    // some tests log in users.
    ASSERT_EQ(extensions::mojom::FeatureSessionType::kInitial,
              extensions::GetCurrentFeatureSessionType());
    session_type_ = extensions::ScopedCurrentFeatureSessionType(
        extensions::GetCurrentFeatureSessionType());

    // Initializes ProfileHelper.
    // TODO(crbug.com/40225390): Migrate into BrowserContextHelper.
    ash::ProfileHelper::Get();
#endif
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    content::RunAllTasksUntilIdle();
#if BUILDFLAG(IS_CHROMEOS)
    session_type_.reset();
    wallpaper_controller_client_.reset();
#endif
  }

 protected:
  virtual std::unique_ptr<ProfileManager> CreateProfileManagerForTest() {
    return std::make_unique<FakeProfileManager>(temp_dir_.GetPath());
  }

  // Helper function to create a profile at `path` for a profile `manager`.
  void CreateProfileAsync(ProfileManager* manager,
                          const base::FilePath& profile_path,
                          MockObserver* mock_observer) {
    manager->CreateProfileAsync(
        profile_path,
        base::BindOnce(&MockObserver::OnProfileInitialized,
                       base::Unretained(mock_observer)),
        base::BindOnce(&MockObserver::OnProfileCreated,
                       base::Unretained(mock_observer)));
  }

#if !BUILDFLAG(IS_ANDROID)
  // Helper function to create a profile with |name| for a profile |manager|.
  void CreateMultiProfileAsync(ProfileManager* manager,
                               const std::string& name,
                               MockObserver* mock_observer) {
    ProfileManager::CreateMultiProfileAsync(
        base::UTF8ToUTF16(name), /*icon_index=*/0, /*is_hidden=*/false,
        base::BindOnce(&MockObserver::OnProfileInitialized,
                       base::Unretained(mock_observer)),
        base::BindOnce(&MockObserver::OnProfileCreated,
                       base::Unretained(mock_observer)));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Helper function to add a profile with |profile_name| to |profile_manager|'s
  // ProfileAttributesStorage, and return the profile created.
  Profile* AddProfileToStorage(ProfileManager* profile_manager,
                               const std::string& path_suffix,
                               const std::u16string& profile_name) {
    ProfileAttributesStorage& storage =
        profile_manager->GetProfileAttributesStorage();
    size_t num_profiles = storage.GetNumberOfProfiles();
    base::FilePath path = temp_dir_.GetPath().AppendASCII(path_suffix);
    ProfileAttributesInitParams params;
    params.profile_path = path;
    params.profile_name = profile_name;
    storage.AddProfile(std::move(params));
    EXPECT_EQ(num_profiles + 1u, storage.GetNumberOfProfiles());
    return profile_manager->GetProfile(path);
  }

  // Helper function to set profile ephemeral at prefs and attributes storage.
  void SetProfileEphemeral(Profile* profile) {
    profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

    // Update IsEphemeral in attributes storage, normally it happened via
    // kForceEphemeralProfiles pref change event routed to
    // ProfileImpl::UpdateIsEphemeralInStorage().
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    ASSERT_NE(entry, nullptr);
    entry->SetIsEphemeral(true);
  }

  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }

#if BUILDFLAG(IS_CHROMEOS)
  // Helper function to register an user with id |user_id| and create profile
  // with a correct path.
  void RegisterUser(const AccountId& account_id) {
    ash::ProfileHelper* profile_helper = ash::ProfileHelper::Get();
    const std::string user_id_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
    user_manager::UserManager::Get()->UserLoggedIn(account_id, user_id_hash,
                                                   false /* browser_restart */,
                                                   false /* is_child */);
    g_browser_process->profile_manager()->GetProfile(
        profile_helper->GetProfilePathByUserIdHash(user_id_hash));
  }

  std::unique_ptr<Profile> InitProfileForArcTransitionTest(
      bool profile_is_new,
      bool arc_signed_in,
      bool profile_is_child,
      bool user_is_child,
      bool profile_is_managed,
      std::optional<bool> arc_is_managed) {
    ash::ProfileHelper* profile_helper = ash::ProfileHelper::Get();
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();

    const std::string user_email = "user_for_transition@example.com";
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(user_email, "1");
    const std::string user_id_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
    const base::FilePath dest_path =
        profile_helper->GetProfilePathByUserIdHash(user_id_hash);

    TestingProfile::Builder builder;
    builder.SetPath(dest_path);
    builder.SetIsNewProfile(profile_is_new);

    if (profile_is_child)
      builder.SetIsSupervisedProfile();

    builder.OverridePolicyConnectorIsManagedForTesting(profile_is_managed);
    std::unique_ptr<Profile> profile = builder.Build();

    profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, arc_signed_in);

    if (arc_is_managed.has_value()) {
      profile->GetPrefs()->SetBoolean(arc::prefs::kArcIsManaged,
                                      *arc_is_managed);
    }

    user_manager->UserLoggedIn(account_id, user_id_hash,
                               false /* browser_restart */, user_is_child);
    g_browser_process->profile_manager()->InitProfileUserPrefs(profile.get());

    return profile;
  }

  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
#endif

  // The path to temporary directory used to contain the test operations. These
  // come before |task_environment_| to avoid issues around backend threads
  // still using the temp directories upon teardown.
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS)
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          local_state_.Get(),
          ash::CrosSettings::Get())};
  std::unique_ptr<base::AutoReset<extensions::mojom::FeatureSessionType>>
      session_type_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
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

MATCHER(SameNotNull, "The same non-NULL value for all calls.") {
  if (!g_created_profile && arg)
    g_created_profile = arg;
  return arg && arg == g_created_profile;
}

#if BUILDFLAG(IS_CHROMEOS)

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
  auto* user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager enabler(base::WrapUnique(user_manager));

  const user_manager::User* active_user =
      user_manager->AddUser(test_account_id);
  user_manager->LoginUser(test_account_id);
  user_manager->SwitchActiveUser(test_account_id);

  base::FilePath expected_logged_in(
      ash::ProfileHelper::GetUserProfileDir(active_user->username_hash()));
  EXPECT_EQ(expected_logged_in.value(),
            profile_manager->GetInitialProfileDir().value());
  VLOG(1) << temp_dir_.GetPath()
                 .Append(profile_manager->GetInitialProfileDir())
                 .value();
}

// Test Get[ActiveUser|PrimaryUser|LastUsed]Profile does not load user profile.
TEST_F(ProfileManagerTest, UserProfileLoading) {
  using ::ash::ProfileHelper;

  Profile* const signin_profile = ProfileHelper::GetSigninProfile();

  // Get[Active|Primary|LastUsed]Profile return the sign-in profile before login
  // happens. IsSameOrParent() is used to properly test against TestProfile
  // whose OTR version uses a different temp path.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->IsSameOrParent(signin_profile));
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->IsSameOrParent(signin_profile));
  EXPECT_TRUE(
      ProfileManager::GetLastUsedProfile()->IsSameOrParent(signin_profile));

  // User signs in but user profile loading has not started.
  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("test-user@example.com", "0123456789");
  const std::string user_id_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  user_manager::UserManager::Get()->UserLoggedIn(account_id, user_id_hash,
                                                 false /* browser_restart */,
                                                 false /* is_child */);

  // Sign-in profile should be returned at this stage. Otherwise, login code
  // ends up in an invalid state. Strange things as in http://crbug.com/728683
  // and http://crbug.com/718734 happens.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->IsSameOrParent(signin_profile));
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->IsSameOrParent(signin_profile));

  // GetLastUsedProfile() after login but before a user profile is loaded is
  // fatal.
  EXPECT_DEATH_IF_SUPPORTED(ProfileManager::GetLastUsedProfile(), ".*");

  // Simulate UserSessionManager loads the profile.
  Profile* const user_profile =
      g_browser_process->profile_manager()->GetProfile(
          ProfileHelper::Get()->GetProfilePathByUserIdHash(user_id_hash));
  ASSERT_FALSE(user_profile->IsSameOrParent(signin_profile));

  // User profile is returned thereafter.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->IsSameOrParent(user_profile));
  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->IsSameOrParent(user_profile));
  EXPECT_TRUE(
      ProfileManager::GetLastUsedProfile()->IsSameOrParent(user_profile));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(ProfileManagerTest, CreateAndUseTwoProfiles) {
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

  // Access some keyed service to simulate use.
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(profile1));
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(profile2));

  // Make sure any pending tasks run before we destroy the profiles.
  content::RunAllTasksUntilIdle();

  TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);

  // Make sure backend sequences clean up correctly.
  content::RunAllTasksUntilIdle();
}

TEST_F(ProfileManagerTest, LoadNonExistingProfile) {
  base::FilePath profile_name(FILE_PATH_LITERAL("NonExistingProfile"));
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->LoadProfile(
      profile_name, false /* incognito */,
      base::BindOnce(&ExpectNullProfile, run_loop_1.QuitClosure()));
  run_loop_1.Run();

  profile_manager->LoadProfile(
      profile_name, true /* incognito */,
      base::BindOnce(&ExpectNullProfile, run_loop_2.QuitClosure()));
  run_loop_2.Run();
}

TEST_F(ProfileManagerTest, LoadExistingProfile) {
  base::FilePath profile_basename(FILE_PATH_LITERAL("MyProfile"));
  base::FilePath profile_path = temp_dir_.GetPath().Append(profile_basename);
  const base::FilePath other_basename(FILE_PATH_LITERAL("SomeOtherProfile"));
  MockObserver mock_observer1;
  EXPECT_CALL(mock_observer1, OnProfileInitialized(SameNotNull()))
      .Times(testing::AtLeast(1));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CreateProfileAsync(profile_manager, profile_path, &mock_observer1);

  // Make sure a real profile is created before continuing.
  content::RunAllTasksUntilIdle();

  base::RunLoop load_profile;
  bool incognito = false;
  profile_manager->LoadProfile(
      profile_basename, incognito,
      base::BindOnce(&ExpectProfileWithName, profile_basename.AsUTF8Unsafe(),
                     incognito, load_profile.QuitClosure()));
  load_profile.Run();

  base::RunLoop load_profile_incognito;
  incognito = true;
  profile_manager->LoadProfile(
      profile_basename, incognito,
      base::BindOnce(&ExpectProfileWithName, profile_basename.AsUTF8Unsafe(),
                     incognito, load_profile_incognito.QuitClosure()));
  load_profile_incognito.Run();

  // Loading some other non existing profile should still return null.
  base::RunLoop load_other_profile;
  profile_manager->LoadProfile(
      other_basename, false,
      base::BindOnce(&ExpectNullProfile, load_other_profile.QuitClosure()));
  load_other_profile.Run();
}

TEST_F(ProfileManagerTest, CreateProfileAsyncMultipleRequests) {
  g_created_profile = nullptr;

  MockObserver mock_observer1;
  EXPECT_CALL(mock_observer1, OnProfileInitialized(SameNotNull()))
      .Times(testing::AtLeast(1));
  MockObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnProfileInitialized(SameNotNull()))
      .Times(testing::AtLeast(1));
  MockObserver mock_observer3;
  EXPECT_CALL(mock_observer3, OnProfileInitialized(SameNotNull()))
      .Times(testing::AtLeast(1));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("New Profile");
  CreateProfileAsync(profile_manager, profile_path, &mock_observer1);
  CreateProfileAsync(profile_manager, profile_path, &mock_observer2);
  CreateProfileAsync(profile_manager, profile_path, &mock_observer3);

  content::RunAllTasksUntilIdle();
}

TEST_F(ProfileManagerTest, CreateProfilesAsync) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath profile_path1 =
      temp_dir_.GetPath().AppendASCII("New Profile 1");
  base::FilePath profile_path2 =
      temp_dir_.GetPath().AppendASCII("New Profile 2");

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .Times(testing::AtLeast(2));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(testing::AtLeast(2));

  CreateProfileAsync(profile_manager, profile_path1, &mock_observer);
  CreateProfileAsync(profile_manager, profile_path2, &mock_observer);

  content::RunAllTasksUntilIdle();
}

// Regression test for https://crbug.com/1472849
TEST_F(ProfileManagerTest, ConcurrentCreationAsyncAndSync) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::_)).Times(0);
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::_)).Times(0);

  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("New Profile");
  CreateProfileAsync(profile_manager, profile_path, &mock_observer);

  // The profile is being created, but creation is not complete.
  EXPECT_EQ(nullptr, profile_manager->GetProfileByPath(profile_path));

  // Request synchronous creation of the same profile, this should not crash.
  Profile* profile_created = nullptr;
  Profile* profile_initialized = nullptr;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&profile_created));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(1)
      .WillOnce(testing::SaveArg<0>(&profile_initialized));
  Profile* profile = profile_manager->GetProfile(profile_path);

  // The profile has been loaded correctly, and all callbacks were called.
  EXPECT_EQ(profile, profile_manager->GetProfileByPath(profile_path));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(profile->GetPath(), profile_path);
  EXPECT_EQ(profile, profile_initialized);
  EXPECT_EQ(profile, profile_created);
}

#if !BUILDFLAG(IS_ANDROID)
// There's no multi-profiles on Android.
TEST_F(ProfileManagerTest, CreateMultiProfileAsync) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  const std::string profile_name = "New Profile";

  base::RunLoop run_loop;
  MockObserver mock_observer;
  Profile* profile = nullptr;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .WillOnce(testing::SaveArg<0>(&profile));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  CreateMultiProfileAsync(profile_manager, profile_name, &mock_observer);
  run_loop.Run();

  // Check that the profile name is set correctly both in the profile prefs and
  // in storage.
  ASSERT_NE(profile, nullptr);
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(base::UTF16ToUTF8(entry->GetName()), profile_name);
  EXPECT_EQ(profile->GetPrefs()->GetString(prefs::kProfileName), profile_name);
}

TEST_F(ProfileManagerTest, CreateMultiProfilesAsync) {
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";

  base::RunLoop run_loop;
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull())).Times(2);
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce(testing::Return())
      .WillOnce([&run_loop] { run_loop.Quit(); });

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  CreateMultiProfileAsync(profile_manager, profile_name1, &mock_observer);
  CreateMultiProfileAsync(profile_manager, profile_name2, &mock_observer);
  run_loop.Run();
}

TEST_F(ProfileManagerTest, CreateMultiProfileAsyncMultipleRequests) {
  base::RunLoop run_loop;
  MockObserver mock_observer;
  Profile *profile1 = nullptr, *profile2 = nullptr, *profile3 = nullptr;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull())).Times(3);
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce(testing::SaveArg<0>(&profile1))
      .WillOnce(testing::SaveArg<0>(&profile2))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&profile3),
                               [&run_loop] { run_loop.Quit(); }));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const std::string profile_name = "New Profile";
  CreateMultiProfileAsync(profile_manager, profile_name, &mock_observer);
  CreateMultiProfileAsync(profile_manager, profile_name, &mock_observer);
  CreateMultiProfileAsync(profile_manager, profile_name, &mock_observer);
  run_loop.Run();

  // A new profile should have been created for each call.
  EXPECT_NE(profile1, profile2);
  EXPECT_NE(profile1, profile3);
  EXPECT_NE(profile2, profile3);
}

TEST_F(ProfileManagerTest,
       CreateMultiProfilesAsyncWithBrokenPrefAndProfileInCache) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Simulate having "Profile 1" in the storage without properly incrementing
  // the prefs::kProfilesNumCreated counter.
  ProfileAttributesInitParams params_1;
  params_1.profile_path =
      profile_manager->user_data_dir().AppendASCII("Profile 1");
  params_1.profile_name = u"name_1";
  params_1.gaia_id = "12345";
  params_1.is_consented_primary_account = true;
  profile_manager->GetProfileAttributesStorage().AddProfile(
      std::move(params_1));

  base::RunLoop run_loop;
  MockObserver mock_observer;
  Profile* profile2 = nullptr;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .WillOnce(testing::SaveArg<0>(&profile2));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  CreateMultiProfileAsync(profile_manager, "Profile B", &mock_observer);
  run_loop.Run();

  ASSERT_NE(profile2, nullptr);
  EXPECT_EQ(profile2->GetPath().BaseName().value(),
            FILE_PATH_LITERAL("Profile 2"));
  ProfileAttributesEntry* entry2 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile2->GetPath());
  ASSERT_NE(entry2, nullptr);
}

TEST_F(ProfileManagerTest,
       PRE_CreateMultiProfilesAsyncWithBrokenPrefAndProfileOnDisk) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  base::RunLoop run_loop;
  MockObserver mock_observer;
  Profile* profile1 = nullptr;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .WillOnce(testing::SaveArg<0>(&profile1));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce([&run_loop] { run_loop.Quit(); });
  CreateMultiProfileAsync(profile_manager, "Profile A", &mock_observer);
  run_loop.Run();

  ASSERT_NE(profile1, nullptr);
  EXPECT_EQ(profile1->GetPath().BaseName().value(),
            FILE_PATH_LITERAL("Profile 1"));
  ProfileAttributesEntry* entry1 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile1->GetPath());
  ASSERT_NE(entry1, nullptr);

  // Decrement next profile number to simulate it was never incremented.
  EXPECT_EQ(local_state()->GetUserPref(prefs::kProfilesNumCreated)->GetInt(),
            2);
  local_state()->SetUserPref(prefs::kProfilesNumCreated,
                             std::make_unique<base::Value>(1));
  // Wipe the profile from profile attributes storage to simulate it got deleted
  // but not wiped from disk.
  profile_manager->GetProfileAttributesStorage().RemoveProfile(
      profile1->GetPath());
}

// We need to restart Chrome in the mean-time to make sure the profile is not
// loaded in memory.
TEST_F(ProfileManagerTest,
       CreateMultiProfilesAsyncWithBrokenPrefAndProfileOnDisk) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  base::RunLoop run_loop;
  MockObserver mock_observer;
  Profile* profile2 = nullptr;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .WillOnce(testing::SaveArg<0>(&profile2));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce([&run_loop] { run_loop.Quit(); });
  CreateMultiProfileAsync(profile_manager, "Profile B", &mock_observer);
  run_loop.Run();

  ASSERT_NE(profile2, nullptr);
  // The profile uses the same base name as in the pre test.
  EXPECT_EQ(profile2->GetPath().BaseName().value(),
            FILE_PATH_LITERAL("Profile 1"));
  ProfileAttributesEntry* entry2 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile2->GetPath());
  ASSERT_NE(entry2, nullptr);
  EXPECT_EQ(base::UTF16ToUTF8(entry2->GetName()), "Profile B");
}

TEST_F(ProfileManagerTest, CreateHiddenProfileAsync) {
  base::RunLoop run_loop;
  Profile* profile = nullptr;
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .WillOnce(testing::SaveArg<0>(&profile));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  profile_manager->CreateMultiProfileAsync(
      u"New Profile", 0, /*is_hidden=*/true,
      base::BindOnce(&MockObserver::OnProfileInitialized,
                     base::Unretained(&mock_observer)),
      base::BindOnce(&MockObserver::OnProfileCreated,
                     base::Unretained(&mock_observer)));

  run_loop.Run();
  ASSERT_NE(profile, nullptr);

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsOmitted());
  EXPECT_TRUE(entry->IsEphemeral());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Checks that the supervised profiles no longer marked as omitted on creation.
TEST_F(ProfileManagerTest, AddProfileToStorageCheckNotOmitted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  const base::FilePath supervised_path =
      temp_dir_.GetPath().AppendASCII("Supervised");
  auto supervised_profile = std::make_unique<TestingProfile>(
      supervised_path, nullptr, Profile::CreateMode::kSynchronous);
  supervised_profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                            supervised_user::kChildAccountSUID);

  // RegisterTestingProfile adds the profile to the attributes storage and takes
  // ownership.
  profile_manager->RegisterTestingProfile(std::move(supervised_profile), true);
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_FALSE(
      storage.GetAllProfilesAttributesSortedByNameWithCheck()[0]->IsOmitted());

  const base::FilePath nonsupervised_path =
      temp_dir_.GetPath().AppendASCII("Non-Supervised");
  auto nonsupervised_profile = std::make_unique<TestingProfile>(
      nonsupervised_path, nullptr, Profile::CreateMode::kSynchronous);
  profile_manager->RegisterTestingProfile(std::move(nonsupervised_profile),
                                          true);

  EXPECT_EQ(2u, storage.GetNumberOfProfiles());
  ProfileAttributesEntry* entry;
  entry = storage.GetProfileAttributesWithPath(supervised_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsOmitted());

  entry = storage.GetProfileAttributesWithPath(nonsupervised_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsOmitted());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(ProfileManagerTest, GetSystemProfilePath) {
  base::FilePath system_profile_path = ProfileManager::GetSystemProfilePath();
  base::FilePath expected_path = temp_dir_.GetPath();
  expected_path = expected_path.Append(chrome::kSystemProfileDir);
  EXPECT_EQ(expected_path, system_profile_path);
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

// Test profile manager that creates all profiles as guest by default.
class UnittestGuestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestGuestProfileManager(const base::FilePath& user_data_dir)
      : FakeProfileManager(user_data_dir) {}

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      Profile::CreateMode create_mode) override {
    TestingProfile::Builder builder;
    if (create_profiles_as_guest_)
      builder.SetGuestSession();
    builder.SetPath(path);
    builder.SetDelegate(delegate);
    builder.SetCreateMode(create_mode);
    return builder.Build();
  }

  void set_create_profiles_as_guest(bool create_profiles_as_guest) {
    create_profiles_as_guest_ = create_profiles_as_guest;
  }

 private:
  bool create_profiles_as_guest_ = true;
};

class ProfileManagerGuestTest : public ProfileManagerTest {
 public:
  ProfileManagerGuestTest() = default;
  ProfileManagerGuestTest(const ProfileManagerGuestTest&) = delete;
  ProfileManagerGuestTest& operator=(const ProfileManagerGuestTest&) = delete;
  ~ProfileManagerGuestTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitch(ash::switches::kGuestSession);
    cl->AppendSwitch(::switches::kIncognito);
#endif

    ProfileManagerTest::SetUp();

#if BUILDFLAG(IS_CHROMEOS)
    RegisterUser(user_manager::GuestAccountId());
#endif
  }

  // Call this function if the test shouldn't create all profiles as guest by
  // default.
  void DoNotCreateNewProfilesAsGuest() {
    unittest_profile_manager_->set_create_profiles_as_guest(false);
  }

 protected:
  std::unique_ptr<ProfileManager> CreateProfileManagerForTest() override {
    auto profile_manager_unique =
        std::make_unique<UnittestGuestProfileManager>(temp_dir_.GetPath());
    unittest_profile_manager_ = profile_manager_unique.get();
    return profile_manager_unique;
  }

#if BUILDFLAG(IS_CHROMEOS)
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#endif

 private:
  raw_ptr<UnittestGuestProfileManager, DanglingUntriaged>
      unittest_profile_manager_ = nullptr;
};

TEST_F(ProfileManagerGuestTest, GetLastUsedProfileAllowedByPolicy) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  Profile* profile = profile_manager->GetLastUsedProfileAllowedByPolicy();
  ASSERT_TRUE(profile);
  EXPECT_TRUE(profile->IsGuestSession());
  EXPECT_TRUE(profile->IsOffTheRecord());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ProfileManagerGuestTest, GuestProfileIncognito) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  EXPECT_TRUE(primary_profile->IsOffTheRecord());

  Profile* active_profile = ProfileManager::GetActiveUserProfile();
  EXPECT_TRUE(active_profile->IsOffTheRecord());

  EXPECT_TRUE(active_profile->IsSameOrParent(primary_profile));

  Profile* last_used_profile = ProfileManager::GetLastUsedProfile();
  EXPECT_TRUE(last_used_profile->IsOffTheRecord());

  EXPECT_TRUE(last_used_profile->IsSameOrParent(active_profile));
}
#endif

TEST_F(ProfileManagerGuestTest, GetGuestProfilePath) {
  base::FilePath guest_path = ProfileManager::GetGuestProfilePath();
  base::FilePath expected_path =
      temp_dir_.GetPath().AppendASCII("Guest Profile");
  EXPECT_EQ(expected_path, guest_path);
}

TEST_F(ProfileManagerGuestTest, GuestProfileAttributes) {
  // In these tests, the primary profile is a guest one.
  Profile* primary_profile = ProfileManager::GetLastUsedProfile();
  ASSERT_TRUE(primary_profile);
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(primary_profile->GetPath());
  EXPECT_EQ(entry, nullptr);
}

TEST_F(ProfileManagerTest, AutoloadProfilesWithBackgroundApps) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  local_state()->SetUserPref(prefs::kBackgroundModeEnabled,
                             std::make_unique<base::Value>(true));

  // Setting a pref which is not applicable to a system (i.e., Android in this
  // case) does not necessarily create it. Don't bother continuing with the
  // test if this pref doesn't exist because it will not load the profiles if
  // it cannot verify that the pref for background mode is enabled.
  if (!local_state()->HasPrefPath(prefs::kBackgroundModeEnabled))
    return;

  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  ProfileAttributesInitParams params_1;
  params_1.profile_path =
      profile_manager->user_data_dir().AppendASCII("path_1");
  params_1.profile_name = u"name_1";
  params_1.gaia_id = "12345";
  params_1.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_1));
  ProfileAttributesInitParams params_2;
  params_2.profile_path =
      profile_manager->user_data_dir().AppendASCII("path_2");
  params_2.profile_name = u"name_2";
  params_2.gaia_id = "23456";
  params_2.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_2));
  ProfileAttributesInitParams params_3;
  params_3.profile_path =
      profile_manager->user_data_dir().AppendASCII("path_3");
  params_3.profile_name = u"name_3";
  params_3.gaia_id = "34567";
  params_3.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_3));

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
  local_state()->SetUserPref(prefs::kBackgroundModeEnabled,
                             std::make_unique<base::Value>(false));

  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  ProfileAttributesInitParams params_1;
  params_1.profile_path =
      profile_manager->user_data_dir().AppendASCII("path_1");
  params_1.profile_name = u"name_1";
  params_1.gaia_id = "12345";
  params_1.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_1));
  ProfileAttributesInitParams params_2;
  params_2.profile_path =
      profile_manager->user_data_dir().AppendASCII("path_2");
  params_2.profile_name = u"name_2";
  params_2.gaia_id = "23456";
  params_2.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_2));

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

// Tests that a new profile's entry in the profile attributes storage is setup
// with the same values that are in the profile prefs.
TEST_F(ProfileManagerTest, InitProfileAttributesStorageForAProfile) {
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

  ProfileAttributesEntry* entry = profile_manager->GetProfileAttributesStorage()
                                      .GetProfileAttributesWithPath(dest_path);
  ASSERT_NE(entry, nullptr);

  // Check if the profile prefs are the same as the storage prefs.
  EXPECT_EQ(profile_name, base::UTF16ToUTF8(entry->GetName()));
  EXPECT_EQ(avatar_index, entry->GetAvatarIconIndex());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ProfileManagerTest, InitProfileForChildOnFirstSignIn) {
  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      true /* profile_is_new */, false /* arc_signed_in */,
      false /* profile_is_child */, true /* user_is_child */,
      false /* profile_is_managed */, false /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::NO_TRANSITION));
  EXPECT_EQ(profile->GetPrefs()->GetString(prefs::kSupervisedUserId),
            supervised_user::kChildAccountSUID);
}

TEST_F(ProfileManagerTest, InitProfileForRegularToChildTransition) {
  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      false /* profile_is_new */, true /* arc_signed_in */,
      false /* profile_is_child */, true /* user_is_child */,
      false /* profile_is_managed */, false /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::REGULAR_TO_CHILD));
  EXPECT_EQ(profile->GetPrefs()->GetString(prefs::kSupervisedUserId),
            supervised_user::kChildAccountSUID);
}

TEST_F(ProfileManagerTest, InitProfileForChildToRegularTransition) {
  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      false /* profile_is_new */, true /* arc_signed_in */,
      true /* profile_is_child */, false /* user_is_child */,
      true /* profile_is_managed */, false /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::CHILD_TO_REGULAR));
  EXPECT_TRUE(profile->GetPrefs()->GetString(prefs::kSupervisedUserId).empty());
}

TEST_F(ProfileManagerTest, InitProfileForUnmanagedToManagedTransition) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      arc::kEnableUnmanagedToManagedTransitionFeature);

  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      false /* profile_is_new */, true /* arc_signed_in */,
      false /* profile_is_child */, false /* user_is_child */,
      true /* profile_is_managed */, false /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::UNMANAGED_TO_MANAGED));
}

TEST_F(ProfileManagerTest, InitProfileForManagedUserOnFirstSignIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      arc::kEnableUnmanagedToManagedTransitionFeature);

  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      true /* profile_is_new */, false /* arc_signed_in */,
      false /* profile_is_child */, false /* user_is_child */,
      true /* profile_is_managed */, false /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::NO_TRANSITION));
}

TEST_F(ProfileManagerTest,
       InitProfileForChildToRegularTransitionArcNotSignedIn) {
  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      false /* profile_is_new */, false /* arc_signed_in */,
      true /* profile_is_child */, false /* user_is_child */,
      true /* profile_is_managed */, false /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::NO_TRANSITION));
  EXPECT_TRUE(profile->GetPrefs()->GetString(prefs::kSupervisedUserId).empty());
}

TEST_F(ProfileManagerTest,
       InitProfileForManagedUserForFirstSignInOnNewVersion) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      arc::kEnableUnmanagedToManagedTransitionFeature);

  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      false /* profile_is_new */, true /* arc_signed_in */,
      false /* profile_is_child */, false /* user_is_child */,
      true /* profile_is_managed */, std::nullopt /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::NO_TRANSITION));
}

TEST_F(ProfileManagerTest, InitProfileForChildUserForFirstSignInOnNewVersion) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      arc::kEnableUnmanagedToManagedTransitionFeature);

  std::unique_ptr<Profile> profile = InitProfileForArcTransitionTest(
      false /* profile_is_new */, true /* arc_signed_in */,
      true /* profile_is_child */, true /* user_is_child */,
      true /* profile_is_managed */, std::nullopt /* arc_is_managed */);

  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(arc::prefs::kArcManagementTransition),
      static_cast<int>(arc::ArcManagementTransition::NO_TRANSITION));
  EXPECT_EQ(profile->GetPrefs()->GetString(prefs::kSupervisedUserId),
            supervised_user::kChildAccountSUID);
}

#endif

TEST_F(ProfileManagerTest, GetLastUsedProfileAllowedByPolicy) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

#if BUILDFLAG(IS_CHROMEOS)
  // On CrOS, profile returned by GetLastUsedProfile is a sign-in profile that
  // is forced to be off-the-record. That's why we need to create at least one
  // user to get a regular profile.
  RegisterUser(
      AccountId::FromUserEmailGaiaId("test-user@example.com", "1234567890"));
#endif

  Profile* profile = profile_manager->GetLastUsedProfileAllowedByPolicy();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->IsOffTheRecord());
  PrefService* prefs = profile->GetPrefs();
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability,
            IncognitoModePrefs::GetAvailability(prefs));

  ASSERT_TRUE(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  IncognitoModePrefs::SetAvailability(
      prefs, policy::IncognitoModeAvailability::kDisabled);
  EXPECT_FALSE(
      profile_manager->GetLastUsedProfileAllowedByPolicy()->IsOffTheRecord());

  // GetLastUsedProfileAllowedByPolicy() returns the off-the-record Profile when
  // incognito mode is forced.
  IncognitoModePrefs::SetAvailability(
      prefs, policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(
      profile_manager->GetLastUsedProfileAllowedByPolicy()->IsOffTheRecord());
}

#if !BUILDFLAG(IS_ANDROID)
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
  EXPECT_FALSE(profile_manager->has_updated_last_opened_profiles());

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(profile1, true);
  std::unique_ptr<Browser> browser1a(
      CreateBrowserWithTestWindowForParams(profile1_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_TRUE(profile_manager->has_updated_last_opened_profiles());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  Browser::CreateParams profile2_params(profile2, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(profile2_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_TRUE(profile_manager->has_updated_last_opened_profiles());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Adding more browsers doesn't change anything.
  std::unique_ptr<Browser> browser1b(
      CreateBrowserWithTestWindowForParams(profile1_params));
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_TRUE(profile_manager->has_updated_last_opened_profiles());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Close the browsers.
  browser1a.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_TRUE(profile_manager->has_updated_last_opened_profiles());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  browser1b.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_TRUE(profile_manager->has_updated_last_opened_profiles());
  EXPECT_EQ(profile2, last_opened_profiles[0]);

  // `has_updated_last_opened_profiles()` should return true even after all
  // profiles have been cleared from the list.
  browser2.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());
  EXPECT_TRUE(profile_manager->has_updated_last_opened_profiles());
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
      CreateBrowserWithTestWindowForParams(profile1_params));

  // And for profile2.
  Browser::CreateParams profile2_params(profile2, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(profile2_params));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Simulate a shutdown.
  chrome::OnClosingAllBrowsers(true);

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
      CreateBrowserWithTestWindowForParams(profile1_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  Browser::CreateParams profile2_params(
      profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true), true);
  std::unique_ptr<Browser> browser2a(
      CreateBrowserWithTestWindowForParams(profile2_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // Adding more browsers doesn't change anything.
  std::unique_ptr<Browser> browser2b(
      CreateBrowserWithTestWindowForParams(profile2_params));
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
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
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
      CreateBrowserWithTestWindowForParams(profile_params));
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
      CreateBrowserWithTestWindowForParams(profile1_params));

  // Create browsers for the ephemeral profile.
  Browser::CreateParams profile2_params(ephemeral_profile1, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(profile2_params));

  Browser::CreateParams profile3_params(ephemeral_profile2, true);
  std::unique_ptr<Browser> browser3(
      CreateBrowserWithTestWindowForParams(profile3_params));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(normal_profile, last_opened_profiles[0]);
  EXPECT_EQ(ephemeral_profile2, last_opened_profiles[1]);

  // Mark the second profile ephemeral.
  SetProfileEphemeral(ephemeral_profile2);

  // Simulate a shutdown.
  chrome::OnClosingAllBrowsers(true);
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
  ProfileAttributesInitParams params_1;
  params_1.profile_path = path1;
  params_1.profile_name = base::UTF8ToUTF16(profile_name1);
  params_1.user_name = base::UTF8ToUTF16(profile_name1);
  params_1.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_1));
  storage.GetAllProfilesAttributes()[0]->SetIsEphemeral(true);
  ASSERT_TRUE(base::CreateDirectory(path1));

  const std::string profile_name2 = "Marge";
  base::FilePath path2 =
      profile_manager->user_data_dir().AppendASCII(profile_name2);
  ProfileAttributesInitParams params_2;
  params_2.profile_path = path2;
  params_2.profile_name = base::UTF8ToUTF16(profile_name2);
  params_2.user_name = base::UTF8ToUTF16(profile_name2);
  params_2.is_consented_primary_account = true;
  storage.AddProfile(std::move(params_2));
  ASSERT_EQ(2u, storage.GetNumberOfProfiles());
  ASSERT_TRUE(base::CreateDirectory(path2));

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_name1);

  // Set the last used profiles.
  ScopedListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::Value::List& initial_last_active_profile_list = update.Get();
  initial_last_active_profile_list.Append(
      base::Value(path1.BaseName().MaybeAsASCII()));
  initial_last_active_profile_list.Append(
      base::Value(path2.BaseName().MaybeAsASCII()));

  profile_manager->GetDeleteProfileHelper().CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();
  const base::Value::List& final_last_active_profile_list =
      local_state->GetList(prefs::kProfilesLastActive);

  // The ephemeral profile should be deleted, and the last used profile set to
  // the other one. Also, the ephemeral profile should be removed from the
  // kProfilesLastActive list.
  EXPECT_FALSE(base::DirectoryExists(path1));
  EXPECT_TRUE(base::DirectoryExists(path2));
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  ASSERT_EQ(1u, final_last_active_profile_list.size());
  ASSERT_EQ(path2.BaseName().MaybeAsASCII(),
            (final_last_active_profile_list)[0].GetString());

  // Mark the remaining profile ephemeral and clean up.
  storage.GetAllProfilesAttributes()[0]->SetIsEphemeral(true);
  profile_manager->GetDeleteProfileHelper().CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();

  // The profile should be deleted, and the last used profile set to a new one.
  EXPECT_FALSE(base::DirectoryExists(path2));
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());
  EXPECT_EQ("Profile 1", local_state->GetString(prefs::kProfileLastUsed));
  ASSERT_EQ(0u, final_last_active_profile_list.size());
}

TEST_F(ProfileManagerGuestTest, CleanUpOnlyEphemeralProfiles) {
  // Create two profiles, one of them is guest.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ASSERT_EQ(0u, storage.GetNumberOfProfiles());

  // Create the guest profile and register it.
  base::FilePath guest_path = ProfileManager::GetGuestProfilePath();
  const std::string guest_profile_name = guest_path.BaseName().MaybeAsASCII();
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(guest_path);
  builder.SetProfileName(guest_profile_name);
  std::unique_ptr<TestingProfile> guest_profile = builder.Build();
  profile_manager->RegisterTestingProfile(std::move(guest_profile), true);

  // Create a regular profile.
  const std::string profile_name = "Homer";
  base::FilePath path =
      profile_manager->user_data_dir().AppendASCII(profile_name);
  ProfileAttributesInitParams params;
  params.profile_path = path;
  params.profile_name = base::UTF8ToUTF16(profile_name);
  params.user_name = base::UTF8ToUTF16(profile_name);
  params.is_consented_primary_account = true;
  storage.AddProfile(std::move(params));
  ASSERT_TRUE(base::CreateDirectory(path));

  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, guest_profile_name);

  // Set the last used profiles.
  ScopedListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::Value::List& initial_last_active_profile_list = update.Get();
  initial_last_active_profile_list.Append(
      base::Value(guest_path.BaseName().MaybeAsASCII()));
  initial_last_active_profile_list.Append(
      base::Value(path.BaseName().MaybeAsASCII()));

  profile_manager->GetDeleteProfileHelper().CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();
  const base::Value::List& final_last_active_profile_list =
      local_state->GetList(prefs::kProfilesLastActive);

  // The guest and the non-ephemeral regular profile aren't impacted.
  EXPECT_TRUE(base::DirectoryExists(guest_path));
  EXPECT_TRUE(base::DirectoryExists(path));
  EXPECT_EQ(guest_profile_name,
            local_state->GetString(prefs::kProfileLastUsed));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  ASSERT_EQ(2u, final_last_active_profile_list.size());
  ASSERT_EQ(guest_path.BaseName().MaybeAsASCII(),
            (final_last_active_profile_list)[0].GetString());
}

TEST_F(ProfileManagerTest, CleanUpEphemeralProfilesWithGuestLastUsedProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ASSERT_EQ(0u, storage.GetNumberOfProfiles());

  const std::string profile_name1 = "Homer";
  base::FilePath path1 =
      profile_manager->user_data_dir().AppendASCII(profile_name1);
  ProfileAttributesInitParams params;
  params.profile_path = path1;
  params.profile_name = base::UTF8ToUTF16(profile_name1);
  params.user_name = base::UTF8ToUTF16(profile_name1);
  params.is_consented_primary_account = true;
  storage.AddProfile(std::move(params));
  storage.GetAllProfilesAttributes()[0]->SetIsEphemeral(true);
  ASSERT_TRUE(base::CreateDirectory(path1));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, std::string("Guest Profile"));

  profile_manager->GetDeleteProfileHelper().CleanUpEphemeralProfiles();
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(0u, storage.GetNumberOfProfiles());
  EXPECT_EQ("Profile 1", local_state->GetString(prefs::kProfileLastUsed));
}

TEST_F(ProfileManagerTest, DestroyProfileOnBrowserClose) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDestroyProfileOnBrowserClose);

  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII("New Profile 1");
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII("New Profile 2");

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);
  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  // Create a browser for profile2.
  Browser::CreateParams profile_params2(profile2, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(profile_params2));

  EXPECT_TRUE(profile_manager->IsValidProfile(profile1));
  EXPECT_TRUE(profile_manager->IsValidProfile(profile2));

  // Close the browser for profile2.
  browser2.reset();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(profile_manager->IsValidProfile(profile1));
  EXPECT_FALSE(profile_manager->IsValidProfile(profile2));
}

TEST_F(ProfileManagerTest, DestroyEphemeralProfileOnBrowserClose) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDestroyProfileOnBrowserClose);

  base::FilePath dest_path1 = temp_dir_.GetPath().AppendASCII("New Profile 1");
  base::FilePath dest_path2 = temp_dir_.GetPath().AppendASCII("New Profile 2");

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create 2 ephemeral profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);
  SetProfileEphemeral(profile1);
  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);
  SetProfileEphemeral(profile2);

  content::RunAllTasksUntilIdle();
  Profile* last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile1, last_used_profile);
  EXPECT_NE(profile2, last_used_profile);
  EXPECT_EQ(3u, storage.GetNumberOfProfiles());
  EXPECT_TRUE(base::PathExists(dest_path1));
  EXPECT_TRUE(base::PathExists(dest_path2));

  // Create a browser for profile2.
  Browser::CreateParams profile_params2(profile2, true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(profile_params2));

  ProfileDeletionWaiter waiter(profile2);
  // Close the browser for profile2.
  browser2.reset();
  waiter.Wait();

  last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile1, last_used_profile);
  EXPECT_NE(profile2, last_used_profile);
  EXPECT_EQ(2u, storage.GetNumberOfProfiles());
  EXPECT_TRUE(base::PathExists(dest_path1));
  // |dest_path2| should've been cleaned up from disk.
  EXPECT_FALSE(base::PathExists(dest_path2));
}

TEST_F(ProfileManagerTest, ActiveProfileDeleted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load two profiles.
  const std::string profile_basename1 = "New Profile 1";
  const std::string profile_basename2 = "New Profile 2";
  base::FilePath profile_path1 =
      temp_dir_.GetPath().AppendASCII(profile_basename1);
  base::FilePath profile_path2 =
      temp_dir_.GetPath().AppendASCII(profile_basename2);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(testing::AtLeast(2));
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .Times(testing::AtLeast(2));

  CreateProfileAsync(profile_manager, profile_path1, &mock_observer);
  CreateProfileAsync(profile_manager, profile_path2, &mock_observer);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(
      2u, profile_manager->GetProfileAttributesStorage().GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_basename1);

  // Delete the active profile.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path1, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(profile_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_basename2, local_state->GetString(prefs::kProfileLastUsed));
}

TEST_F(ProfileManagerTest, LastProfileDeleted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create and load a profile.
  const std::string profile_basename1 = "New Profile 1";
  base::FilePath profile_path1 =
      temp_dir_.GetPath().AppendASCII(profile_basename1);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(testing::AtLeast(1));

  CreateProfileAsync(profile_manager, profile_path1, &mock_observer);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Set it as the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_basename1);

  // Delete the active profile.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path1, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  // A new profile should have been created
  const std::string profile_basename2 = "Profile 1";
  base::FilePath profile_path2 =
      temp_dir_.GetPath().AppendASCII(profile_basename2);

  EXPECT_EQ(profile_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_basename2, local_state->GetString(prefs::kProfileLastUsed));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(profile_path2, storage.GetAllProfilesAttributes()[0]->GetPath());
}

TEST_F(ProfileManagerGuestTest, LastProfileDeletedWithGuestActiveProfile) {
  // Make new profiles to be created as non-guest by default.
  DoNotCreateNewProfilesAsGuest();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  // Create and load a profile.
  const std::string profile_basename1 = "New Profile 1";
  base::FilePath profile_path1 =
      temp_dir_.GetPath().AppendASCII(profile_basename1);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(testing::AtLeast(1));

  CreateProfileAsync(profile_manager, profile_path1, &mock_observer);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Create the profile and register it.
  const std::string guest_profile_basename =
      ProfileManager::GetGuestProfilePath().BaseName().MaybeAsASCII();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());
  std::unique_ptr<TestingProfile> guest_profile = builder.Build();
  guest_profile->set_profile_name(guest_profile_basename);
  profile_manager->RegisterTestingProfile(std::move(guest_profile), false);

  // The Guest profile does not get added to the ProfileAttributesStorage.
  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, storage.GetNumberOfProfiles());

  // Set the Guest profile as the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, guest_profile_basename);

  // Delete the other profile.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path1, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  // A new profile should have been created.
  const std::string profile_basename2 = "Profile 1";
  base::FilePath profile_path2 =
      temp_dir_.GetPath().AppendASCII(profile_basename2);

  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose))
    EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  else
    EXPECT_EQ(3u, profile_manager->GetLoadedProfiles().size());
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  EXPECT_EQ(profile_path2, storage.GetAllProfilesAttributes()[0]->GetPath());
}

TEST_F(ProfileManagerTest, ProfileDisplayNameResetsDefaultName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const std::u16string default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const std::u16string profile_name1 = storage.ChooseNameForNewProfile(0u);
  Profile* profile1 = AddProfileToStorage(profile_manager,
                                          "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const std::u16string profile_name2 = storage.ChooseNameForNewProfile(1u);
  Profile* profile2 = AddProfileToStorage(profile_manager,
                                          "path_2", profile_name2);
  EXPECT_EQ(profile_name1,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the default name.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile2->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
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
  const std::u16string default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const std::u16string profile_name1 = storage.ChooseNameForNewProfile(0u);
  Profile* profile1 = AddProfileToStorage(profile_manager,
                                          "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  ASSERT_EQ(1u, storage.GetNumberOfProfiles());

  // We should display custom names for local profiles.
  const std::u16string custom_profile_name = u"Batman";
  ProfileAttributesEntry* entry = storage.GetAllProfilesAttributes()[0];
  entry->SetLocalProfileName(custom_profile_name, false);
  EXPECT_EQ(custom_profile_name, entry->GetName());
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const std::u16string profile_name2 = storage.ChooseNameForNewProfile(1u);
  Profile* profile2 = AddProfileToStorage(profile_manager,
                                          "path_2", profile_name2);
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the original, custom name.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile2->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
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
  const std::u16string default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const std::u16string profile_name1 = storage.ChooseNameForNewProfile(0u);
  Profile* profile1 = AddProfileToStorage(profile_manager,
                                          "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  ASSERT_EQ(1u, storage.GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage.GetAllProfilesAttributes()[0];
  // For a signed in profile with a default name we still display
  // IDS_SINGLE_PROFILE_DISPLAY_NAME.
  entry->SetAuthInfo("12345", u"user@gmail.com", true);
  EXPECT_EQ(profile_name1, entry->GetName());
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // For a signed in profile with a non-default Gaia given name we display the
  // Gaia given name.
  entry->SetAuthInfo("12345", u"user@gmail.com", true);
  const std::u16string gaia_given_name(u"given name");
  entry->SetGAIAGivenName(gaia_given_name);
  EXPECT_EQ(gaia_given_name, entry->GetName());
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const std::u16string profile_name2 = storage.ChooseNameForNewProfile(1u);
  Profile* profile2 = AddProfileToStorage(profile_manager,
                                          "path_2", profile_name2);
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the original, actual profile name.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile2->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

// GetAvatarNameForProfile() is not defined on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ProfileManagerTest, ProfileDisplayNameIsEmailIfDefaultName) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  EXPECT_EQ(0u, storage.GetNumberOfProfiles());

  // Create two signed in profiles, with both new and legacy default names, and
  // a profile with a custom name.
  Profile* profile1 =
      AddProfileToStorage(profile_manager, "path_1", u"Person 1");
  Profile* profile2 =
      AddProfileToStorage(profile_manager, "path_2", u"Default Profile");
  const std::u16string profile_name3(u"Batman");
  Profile* profile3 = AddProfileToStorage(profile_manager, "path_3",
                                          profile_name3);
  EXPECT_EQ(3u, storage.GetNumberOfProfiles());

  // Sign in all profiles, and make sure they do not have a Gaia name set.
  const std::u16string email1(u"user1@gmail.com");
  const std::u16string email2(u"user2@gmail.com");
  const std::u16string email3(u"user3@gmail.com");

  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile1->GetPath());

  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo("12345", email1, true);
  entry->SetGAIAGivenName(std::u16string());
  entry->SetGAIAName(std::u16string());

  entry = storage.GetProfileAttributesWithPath(profile2->GetPath());
  ASSERT_NE(entry, nullptr);
#if !BUILDFLAG(IS_CHROMEOS)
  // (Default profile, Batman,..) are legacy profile names on Desktop and are
  // not considered default profile names for newly created profiles.
  // We use "Person %n" as the default profile name. Set |SetIsUsingDefaultName|
  // manually to mimick pre-existing profiles.
  entry->SetLocalProfileName(u"Default Profile", true);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  entry->SetAuthInfo("23456", email2, true);
  entry->SetGAIAGivenName(std::u16string());
  entry->SetGAIAName(std::u16string());

  entry = storage.GetProfileAttributesWithPath(profile3->GetPath());
  ASSERT_NE(entry, nullptr);

  entry->SetAuthInfo("34567", email3, true);
  entry->SetGAIAGivenName(std::u16string());
  entry->SetGAIAName(std::u16string());

  // The profiles with default names should display the email address.
  EXPECT_EQ(email1, profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(email2, profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // The profile with the custom name should display that.
  EXPECT_EQ(profile_name3,
            profiles::GetAvatarNameForProfile(profile3->GetPath()));

  // Adding a Gaia name to a profile that previously had a default name should
  // start displaying it.
  const std::u16string gaia_given_name(u"Robin");
  entry = storage.GetProfileAttributesWithPath(profile1->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetGAIAGivenName(gaia_given_name);
  EXPECT_EQ(gaia_given_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// These tests are for a Mac-only code path that assumes the browser
// process isn't killed when all browser windows are closed.
TEST_F(ProfileManagerTest, ActiveProfileDeletedNeedsToLoadNextProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load one profile, and just create a second profile.
  const std::string profile_basename1 = "New Profile 1";
  const std::string profile_basename2 = "New Profile 2";
  base::FilePath profile_path1 =
      temp_dir_.GetPath().AppendASCII(profile_basename1);
  base::FilePath profile_path2 =
      temp_dir_.GetPath().AppendASCII(profile_basename2);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(testing::AtLeast(1));
  CreateProfileAsync(profile_manager, profile_path1, &mock_observer);
  content::RunAllTasksUntilIdle();

  // Track the profile, but don't load it.
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesInitParams params;
  params.profile_path = profile_path2;
  params.profile_name = ASCIIToUTF16(profile_basename2);
  params.gaia_id = "23456";
  params.is_consented_primary_account = true;
  storage.AddProfile(std::move(params));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(2u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_basename1);

  // Delete the active profile. This should switch and load the unloaded
  // profile.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path1, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(profile_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_basename2, local_state->GetString(prefs::kProfileLastUsed));
}

// This tests the recursive call in ProfileManager::OnNewActiveProfileLoaded
// by simulating a scenario in which the profile that is being loaded as
// the next active profile has also been marked for deletion, so the
// ProfileManager needs to recursively select a different next profile.
TEST_F(ProfileManagerTest, ActiveProfileDeletedNextProfileDeletedToo) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load one profile, and create two more profiles.
  const std::string profile_basename1 = "New Profile 1";
  const std::string profile_basename2 = "New Profile 2";
  const std::string profile_basename3 = "New Profile 3";
  base::FilePath profile_path1 =
      temp_dir_.GetPath().AppendASCII(profile_basename1);
  base::FilePath profile_path2 =
      temp_dir_.GetPath().AppendASCII(profile_basename2);
  base::FilePath profile_path3 =
      temp_dir_.GetPath().AppendASCII(profile_basename3);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(testing::NotNull()))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(mock_observer, OnProfileInitialized(testing::NotNull()))
      .Times(testing::AtLeast(1));
  CreateProfileAsync(profile_manager, profile_path1, &mock_observer);
  content::RunAllTasksUntilIdle();

  // Create the other profiles, but don't load them. Assign a fake avatar icon
  // to ensure that profiles in the profile attributes storage are sorted by the
  // profile name, and not randomly by the avatar name.
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesInitParams params2;
  params2.profile_path = profile_path2;
  params2.profile_name = ASCIIToUTF16(profile_basename2);
  params2.gaia_id = "23456";
  params2.user_name = ASCIIToUTF16(profile_basename2);
  params2.is_consented_primary_account = true;
  params2.icon_index = 1;
  storage.AddProfile(std::move(params2));
  ProfileAttributesInitParams params3;
  params3.profile_path = profile_path3;
  params3.profile_name = ASCIIToUTF16(profile_basename3);
  params3.gaia_id = "34567";
  params3.user_name = ASCIIToUTF16(profile_basename3);
  params3.is_consented_primary_account = true;
  params3.icon_index = 2;
  storage.AddProfile(std::move(params3));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(3u, storage.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_path1.BaseName().MaybeAsASCII());

  // Delete the active profile, Profile1.
  // This will post a CreateProfileAsync message, that tries to load Profile2,
  // which checks that the profile is not being deleted, and then calls back
  // FinishDeletingProfile for Profile1.
  // Try to break this flow by setting the active profile to Profile2 in the
  // middle (so after the first posted message), and trying to delete Profile2,
  // so that the ProfileManager has to look for a different profile to load.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path1, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  local_state->SetString(prefs::kProfileLastUsed,
                         profile_path2.BaseName().MaybeAsASCII());
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path2, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(profile_path3, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_basename3, local_state->GetString(prefs::kProfileLastUsed));
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(ProfileManagerTest, CannotCreateProfileOutsideUserDir) {
  base::ScopedTempDir non_user_dir;
  ASSERT_TRUE(non_user_dir.CreateUniqueTempDir());

  base::FilePath dest_path = non_user_dir.GetPath();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  Profile* profile = profile_manager->GetProfile(dest_path);
  EXPECT_EQ(nullptr, profile);
}

TEST_F(ProfileManagerTest, CannotCreateProfileOutsideUserDirAsync) {
  base::ScopedTempDir non_user_dir;
  ASSERT_TRUE(non_user_dir.CreateUniqueTempDir());

  base::FilePath profile_path =
      non_user_dir.GetPath().AppendASCII("New Profile");

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileInitialized(nullptr));

  CreateProfileAsync(profile_manager, profile_path, &mock_observer);
  content::RunAllTasksUntilIdle();
}

TEST_F(ProfileManagerTest, ScopedProfileKeepAlive) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kDestroyProfileOnBrowserClose);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath dest_path = temp_dir_.GetPath().AppendASCII("New Profile");
  Profile* profile = profile_manager->GetProfile(dest_path);

  // Starts with a kWaitingForFirstBrowserWindow ref.
  EXPECT_THAT(profile_manager->GetProfileInfoByPath(dest_path)->keep_alives,
              ::testing::ElementsAre(std::pair<ProfileKeepAliveOrigin, int>(
                  ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow, 1)));

  {
    // Set |profile| refcount to 1. This will cause the profile to get deleted
    // at the end of this block.
    ScopedProfileKeepAlive keep_alive(profile,
                                      ProfileKeepAliveOrigin::kBrowserWindow);

    // We added the first browser window. There should be no more
    // kWaitingForFirstBrowserWindow ref.
    EXPECT_THAT(
        profile_manager->GetProfileInfoByPath(dest_path)->keep_alives,
        ::testing::UnorderedElementsAre(
            std::pair<ProfileKeepAliveOrigin, int>(
                ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow, 0),
            std::pair<ProfileKeepAliveOrigin, int>(
                ProfileKeepAliveOrigin::kBrowserWindow, 1)));
  }

  base::RunLoop().RunUntilIdle();
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Profile* should've been destroyed by now.
  EXPECT_EQ(nullptr, profile_manager->GetProfileByPath(dest_path));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
}

// Tests that a new profile's entry in the profile attributes storage is setup
// with the same values that are in the profile prefs.
TEST_F(ProfileManagerTest, ProfileCountRecordedAtProfileInit) {
  using base::Bucket;
  using base::BucketsAre;

  base::HistogramTester histogram_tester;
  const std::string kHistogramName =
      "Profile.NumberOfProfilesAtProfileCreation";
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath dest_path = temp_dir_.GetPath();

  base::FilePath path_1 = dest_path.Append(FILE_PATH_LITERAL("Profile 1"));
  profile_manager->GetProfile(path_1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
              BucketsAre(Bucket(1, 1)));

  Profile* profile_2 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("Profile 2")));
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 2u);
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
              BucketsAre(Bucket(1, 1), Bucket(2, 1)));

  // Incognito profile should not affect the count.
  EXPECT_TRUE(profile_2->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
              BucketsAre(Bucket(1, 1), Bucket(2, 1)));

#if !BUILDFLAG(IS_ANDROID)
  // Delete one profile to decrement the count.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      path_1, base::DoNothing(), ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  content::RunAllTasksUntilIdle();

  profile_manager->GetProfile(dest_path.Append(FILE_PATH_LITERAL("Profile 3")));
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
              BucketsAre(Bucket(1, 1), Bucket(2, 2)));
#endif  // !BUILDFLAG(IS_ANDROID)
}

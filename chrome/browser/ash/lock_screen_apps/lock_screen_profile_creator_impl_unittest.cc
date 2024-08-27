// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/lock_screen_profile_creator_impl.h"

#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/traits_bag.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::ash::ProfileHelper;
using lock_screen_apps::LockScreenProfileCreator;
using lock_screen_apps::LockScreenProfileCreatorImpl;

const char kPrimaryUser[] = "user@user";

std::unique_ptr<arc::ArcSession> ArcSessionFactory() {
  ADD_FAILURE() << "Attempt to create arc session.";
  return nullptr;
}

void SetWasRun(bool* was_run) {
  *was_run = true;
}

// Wrapper around a |Profile::Delegate| used to manage timing of an async
// profile creation callbacks, and to provide a way to override profile creation
// result.
// An instance can be passed to TestingProfile as delegate - when the testing
// profile creation is done, i.e. when the delegate's
// |OnProfileCreationFinished| is called this will remember the result. The
// creation result will be forwarded to the actual (wrapped) delegate when
// |WaitForCreationAndOverrideResponse| is called. This method will additionally
// wait until the profile creation finishes.
class PendingProfileCreation : public Profile::Delegate {
 public:
  PendingProfileCreation() {}

  PendingProfileCreation(const PendingProfileCreation&) = delete;
  PendingProfileCreation& operator=(const PendingProfileCreation&) = delete;

  ~PendingProfileCreation() override {}

  // Sets the pending profile creation to track a profile creation,
  // |path| - the created profile path.
  // |delegate| - the original profile creation delegate - the delegate to
  //     which creation result should be forwarded when appropriate.
  //
  // This will cause test failure if called when the |PendingProfileCreation| is
  // already set up to track a profile. The |PendingProfileCreation| will be
  // reset when the profile creation result is forwarded to |delegate|.
  void Set(const base::FilePath& path, Profile::Delegate* delegate) {
    ASSERT_TRUE(path_.empty());
    ASSERT_FALSE(delegate_);

    path_ = path;
    delegate_ = delegate;
    profile_ = nullptr;

    if (!wait_quit_closure_.is_null()) {
      ADD_FAILURE() << "Wait closure set on reset.";
      RunWaitQuitClosure();
    }
  }

  // Tracked profile path.
  const base::FilePath& path() const { return path_; }

  // Waits for profile creation to finish, and then forwards creation result to
  // the original delegate, with profile creation success value set to
  // |success|. This will return false if the |PendingProfileCreation| is not
  // set to track a profile, or if the profile creation fails.
  bool WaitForCreationAndOverrideResponse(bool success) {
    if (path_.empty() || !delegate_)
      return false;

    if (!profile_) {
      base::RunLoop run_loop;
      wait_quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    if (!profile_)
      return false;

    if (!success_)
      return false;

    path_ = base::FilePath();

    Profile::Delegate* delegate = delegate_;
    delegate_ = nullptr;

    Profile* profile = profile_;
    profile_ = nullptr;

    delegate->OnProfileCreationFinished(
        profile, Profile::CreateMode::kAsynchronous, success, is_new_profile_);
    return true;
  }

  void OnProfileCreationStarted(Profile* profile,
                                Profile::CreateMode create_mode) override {}

  // Called when the profile is created - it caches the result, and quits the
  // run loop potentially set in |WaitForCreationAndOverrideResponse|.
  void OnProfileCreationFinished(Profile* profile,
                                 Profile::CreateMode create_mode,
                                 bool success,
                                 bool is_new_profile) override {
    ASSERT_FALSE(profile_);

    profile_ = profile;
    success_ = success;
    is_new_profile_ = is_new_profile;

    RunWaitQuitClosure();
  }

 private:
  // Quits run loop set in |WaitForCreationAndOverrideResponse|.
  void RunWaitQuitClosure() {
    if (wait_quit_closure_.is_null())
      return;

    std::move(wait_quit_closure_).Run();
  }

  base::FilePath path_;
  raw_ptr<Profile::Delegate> delegate_ = nullptr;
  base::OnceClosure wait_quit_closure_;

  raw_ptr<Profile> profile_ = nullptr;
  bool success_ = false;
  bool is_new_profile_ = false;
};

// Test profile manager implementation used to track async profile creation.
class UnittestProfileManager : public FakeProfileManager {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : FakeProfileManager(user_data_dir) {}

  ~UnittestProfileManager() override = default;

  PendingProfileCreation* pending_profile_creation() {
    return &pending_profile_creation_;
  }

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      Profile::CreateMode create_mode) override {
    pending_profile_creation_.Set(path, delegate);

    auto new_profile = std::make_unique<TestingProfile>(
        path, &pending_profile_creation_, create_mode);

    // Build accompaning incognito profile, to ensure it has the same path
    // as the original profile.
    TestingProfile::Builder incognito_builder;
    incognito_builder.SetPath(path);
    incognito_builder.BuildIncognito(new_profile.get());

    return new_profile;
  }

 private:
  PendingProfileCreation pending_profile_creation_;
};

class LockScreenProfileCreatorImplTest : public testing::Test {
 public:
  LockScreenProfileCreatorImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  LockScreenProfileCreatorImplTest(const LockScreenProfileCreatorImplTest&) =
      delete;
  LockScreenProfileCreatorImplTest& operator=(
      const LockScreenProfileCreatorImplTest&) = delete;

  ~LockScreenProfileCreatorImplTest() override {}

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        crx_file::id_util::GenerateId("test_app"));
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    auto profile_manager_unique =
        std::make_unique<UnittestProfileManager>(user_data_dir_.GetPath());
    profile_manager_ = profile_manager_unique.get();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager_unique));

    CreatePrimaryProfile();

    InitExtensionSystem(primary_profile_);

    apps::WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(primary_profile_));

    // Needed by note taking helper.
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(&ArcSessionFactory)));
    ash::NoteTakingHelper::Initialize();

    lock_screen_profile_creator_ =
        std::make_unique<LockScreenProfileCreatorImpl>(primary_profile_,
                                                       &tick_clock_);
  }

  void TearDown() override {
    lock_screen_profile_creator_.reset();
    arc_session_manager_.reset();
    ash::NoteTakingHelper::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);

    ash::ConciergeClient::Shutdown();
  }

  UnittestProfileManager* profile_manager() { return profile_manager_; }

  Profile* primary_profile() { return primary_profile_; }

  LockScreenProfileCreator* lock_screen_profile_creator() {
    return lock_screen_profile_creator_.get();
  }

  base::SimpleTestTickClock* tick_clock() { return &tick_clock_; }

  extensions::TestExtensionSystem* GetExtensionSystem(Profile* profile) {
    return static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile));
  }

  // Creates a lock screen enabled note taking app.
  scoped_refptr<const extensions::Extension> CreateTestNoteTakingApp() {
    base::Value::Dict background = base::Value::Dict().Set(
        "scripts", base::Value::List().Append("background.js"));
    base::Value::List action_handlers =
        base::Value::List().Append(base::Value::Dict()
                                       .Set("action", "new_note")
                                       .Set("enabled_on_lock_screen", true));

    auto manifest_builder =
        base::Value::Dict()
            .Set("name", "Note taking app")
            .Set("manifest_version", 2)
            .Set("version", "1.1")
            .Set("app",
                 base::Value::Dict().Set("background", std::move(background)))
            .Set("permissions", base::Value::List().Append("lockScreen"))
            .Set("action_handlers", std::move(action_handlers));

    return extensions::ExtensionBuilder()
        .SetManifest(std::move(manifest_builder))
        .SetID(crx_file::id_util::GenerateId("test_app"))
        .Build();
  }

  // Creates a lock screen enabled note taking app and adds it to the profile's
  // extension service.
  scoped_refptr<const extensions::Extension> AddTestNoteTakingApp(
      Profile* profile) {
    scoped_refptr<const extensions::Extension> app = CreateTestNoteTakingApp();
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->AddExtension(app.get());
    return app;
  }

  // Sets |app| as the default note taking app in a  profile, and sets whether
  // it's enabled on the lock screen.
  void SetAppEnabledOnLockScreen(Profile* profile,
                                 const extensions::Extension* app,
                                 bool enabled) {
    ash::NoteTakingHelper::Get()->SetPreferredApp(profile, app->id());
    ash::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(profile,
                                                                     enabled);
  }

  // Marks extension system as ready.
  void SetExtensionSystemReady(Profile* profile) {
    GetExtensionSystem(profile)->SetReady();
    base::RunLoop run_loop;
    GetExtensionSystem(profile)->ready().Post(FROM_HERE,
                                              run_loop.QuitClosure());
    run_loop.Run();
  }

  void RecordLockScreenProfile(Profile** lock_screen_profile) {
    *lock_screen_profile = lock_screen_profile_creator()->lock_screen_profile();
  }

 private:
  void InitExtensionSystem(Profile* profile) {
    GetExtensionSystem(profile)->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        profile->GetPath().Append("Extensions") /* install_directory */,
        false /* autoupdate_enabled */);
  }

  void CreatePrimaryProfile() {
    DCHECK(!scoped_user_manager_) << "there can be only one primary profile";
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryUser));
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    base::FilePath user_profile_path =
        user_data_dir_.GetPath().Append(ProfileHelper::Get()->GetUserProfileDir(
            user_manager::FakeUserManager::GetFakeUsernameHash(account_id)));
    auto profile = std::make_unique<TestingProfile>(user_profile_path);
    primary_profile_ = profile.get();
    profile_manager_->RegisterTestingProfile(std::move(profile),
                                             false /*add_to_storage*/);
    DCHECK(ash::ProfileHelper::IsPrimaryProfile(primary_profile_));
  }

  base::ScopedTempDir user_data_dir_;
  ScopedTestingLocalState local_state_;
  content::BrowserTaskEnvironment task_environment_;

  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  raw_ptr<UnittestProfileManager, DanglingUntriaged> profile_manager_;

  raw_ptr<TestingProfile, DanglingUntriaged> primary_profile_ = nullptr;

  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

  std::unique_ptr<LockScreenProfileCreator> lock_screen_profile_creator_;
};

}  // namespace

TEST_F(LockScreenProfileCreatorImplTest,
       CreateProfileWhenLockScreenNotesEnabled) {
  EXPECT_FALSE(lock_screen_profile_creator()->Initialized());
  lock_screen_profile_creator()->Initialize();

  EXPECT_TRUE(lock_screen_profile_creator()->Initialized());

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_run));

  SetExtensionSystemReady(primary_profile());

  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  EXPECT_FALSE(lock_screen_profile_creator()->lock_screen_profile());
  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(callback_run);

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(lock_screen_profile_creator()->ProfileCreated());

  Profile* lock_screen_profile =
      lock_screen_profile_creator()->lock_screen_profile();
  ASSERT_TRUE(lock_screen_profile);

  EXPECT_TRUE(ProfileHelper::IsLockScreenAppProfile(lock_screen_profile));
  EXPECT_FALSE(
      lock_screen_profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled));
  EXPECT_TRUE(lock_screen_profile->GetPrefs()->GetBoolean(
      prefs::kForceEphemeralProfiles));
  EXPECT_FALSE(ProfileHelper::Get()->GetUserByProfile(lock_screen_profile));

  // `AppManagerImpl` uses the original non-OffTheRecord profile to install apps
  // to the lock screen. Regular web apps and app service should be available,
  // but not system web apps.
  EXPECT_TRUE(lock_screen_profile->IsOffTheRecord());
  EXPECT_TRUE(web_app::WebAppProvider::GetForLocalAppsUnchecked(
      lock_screen_profile->GetOriginalProfile()));
  EXPECT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      lock_screen_profile->GetOriginalProfile()));
  EXPECT_FALSE(
      ash::SystemWebAppManager::Get(lock_screen_profile->GetOriginalProfile()));

  // Profile should not be recreated if lock screen note taking gets re-enabled.
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), false);
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_TRUE(profile_manager()->pending_profile_creation()->path().empty());
}

TEST_F(LockScreenProfileCreatorImplTest, ProfileCreationError) {
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_run));

  SetExtensionSystemReady(primary_profile());

  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(callback_run);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());
  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(false));

  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(lock_screen_profile_creator()->lock_screen_profile());

  // Profile should not be recreated if lock screen note taking gets reenabled.
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), false);
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_TRUE(profile_manager()->pending_profile_creation()->path().empty());
}

TEST_F(LockScreenProfileCreatorImplTest,
       WaitUntilPrimaryExtensionSystemReadyBeforeCreatingProfile) {
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_run));

  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_TRUE(profile_manager()->pending_profile_creation()->path().empty());
  EXPECT_FALSE(lock_screen_profile_creator()->lock_screen_profile());
  EXPECT_FALSE(callback_run);

  SetExtensionSystemReady(primary_profile());

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  EXPECT_FALSE(callback_run);

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(callback_run);
  Profile* lock_screen_profile =
      lock_screen_profile_creator()->lock_screen_profile();
  ASSERT_TRUE(lock_screen_profile);
  EXPECT_TRUE(ProfileHelper::IsLockScreenAppProfile(lock_screen_profile));
}

TEST_F(LockScreenProfileCreatorImplTest, InitializedAfterNoteTakingEnabled) {
  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);
  SetExtensionSystemReady(primary_profile());

  lock_screen_profile_creator()->Initialize();
  base::RunLoop().RunUntilIdle();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_run));

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  EXPECT_FALSE(callback_run);
  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(lock_screen_profile_creator()->lock_screen_profile());

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(callback_run);
  Profile* lock_screen_profile =
      lock_screen_profile_creator()->lock_screen_profile();
  ASSERT_TRUE(lock_screen_profile);
  EXPECT_TRUE(ProfileHelper::IsLockScreenAppProfile(lock_screen_profile));
}

TEST_F(LockScreenProfileCreatorImplTest, MultipleCallbacks) {
  lock_screen_profile_creator()->Initialize();

  bool first_callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &first_callback_run));

  bool second_callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &second_callback_run));

  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());
  SetExtensionSystemReady(primary_profile());
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_FALSE(first_callback_run);
  EXPECT_FALSE(second_callback_run);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  bool callback_added_during_creation_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_added_during_creation_run));

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(lock_screen_profile_creator()->lock_screen_profile());

  EXPECT_TRUE(first_callback_run);
  EXPECT_TRUE(second_callback_run);
  EXPECT_TRUE(callback_added_during_creation_run);

  bool callback_added_after_creation_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_added_after_creation_run));
  EXPECT_TRUE(callback_added_after_creation_run);
}

TEST_F(LockScreenProfileCreatorImplTest, LockScreenProfileSetBeforeCallback) {
  lock_screen_profile_creator()->Initialize();

  Profile* lock_screen_profile = nullptr;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&LockScreenProfileCreatorImplTest::RecordLockScreenProfile,
                     base::Unretained(this), &lock_screen_profile));

  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());
  SetExtensionSystemReady(primary_profile());
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);
  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(lock_screen_profile_creator()->lock_screen_profile());
  EXPECT_EQ(lock_screen_profile_creator()->lock_screen_profile(),
            lock_screen_profile);

  lock_screen_profile = nullptr;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&LockScreenProfileCreatorImplTest::RecordLockScreenProfile,
                     base::Unretained(this), &lock_screen_profile));

  EXPECT_EQ(lock_screen_profile_creator()->lock_screen_profile(),
            lock_screen_profile);
}

TEST_F(LockScreenProfileCreatorImplTest, MetricsOnSuccess) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_run));

  SetExtensionSystemReady(primary_profile());
  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());

  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(callback_run);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  tick_clock()->Advance(base::Milliseconds(20));

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(callback_run);
}

TEST_F(LockScreenProfileCreatorImplTest, MetricsOnFailure) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::BindOnce(&SetWasRun, &callback_run));

  SetExtensionSystemReady(primary_profile());
  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());

  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(callback_run);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  tick_clock()->Advance(base::Milliseconds(20));

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(false));

  EXPECT_TRUE(callback_run);
}

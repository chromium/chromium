// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/lock_screen_profile_creator_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_session.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using chromeos::ProfileHelper;
using extensions::DictionaryBuilder;
using extensions::ListBuilder;
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
// profile creation is done, i.e. when the delegate's |OnProfileCreated| is
// called this will remember the result. The creation result will be forwarded
// to the actual (wrapped) delegate when |WaitForCreationAndOverrideResponse| is
// called. This method will additionally wait until the profile creation
// finishes.
class PendingProfileCreation : public Profile::Delegate {
 public:
  PendingProfileCreation() {}
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

    delegate->OnProfileCreated(profile, success, is_new_profile_);
    return true;
  }

  // Called when the profile is created - it caches the result, and quits the
  // run loop potentially set in |WaitForCreationAndOverrideResponse|.
  void OnProfileCreated(Profile* profile,
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
  Profile::Delegate* delegate_ = nullptr;
  base::Closure wait_quit_closure_;

  Profile* profile_ = nullptr;
  bool success_ = false;
  bool is_new_profile_ = false;

  DISALLOW_COPY_AND_ASSIGN(PendingProfileCreation);
};

// Test profile manager implementation used to track async profile creation.
class UnittestProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

  ~UnittestProfileManager() override {}

  PendingProfileCreation* pending_profile_creation() {
    return &pending_profile_creation_;
  }

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    return std::make_unique<TestingProfile>(path);
  }

  std::unique_ptr<Profile> CreateProfileAsyncHelper(
      const base::FilePath& path,
      Delegate* delegate) override {
    pending_profile_creation_.Set(path, delegate);

    auto new_profile =
        std::make_unique<TestingProfile>(path, &pending_profile_creation_);

    // Build accompaning incognito profile, to ensure it has the same path
    // as the original profile.
    TestingProfile::Builder incognito_builder;
    incognito_builder.SetPath(path);
    incognito_builder.BuildIncognito(new_profile.get());

    return new_profile;
  }

 private:
  PendingProfileCreation pending_profile_creation_;

  DISALLOW_COPY_AND_ASSIGN(UnittestProfileManager);
};

class LockScreenProfileCreatorImplTest : public testing::Test {
 public:
  LockScreenProfileCreatorImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~LockScreenProfileCreatorImplTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        crx_file::id_util::GenerateId("test_app"));
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    auto profile_manager =
        std::make_unique<UnittestProfileManager>(user_data_dir_.GetPath());
    profile_manager_ = profile_manager.get();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        profile_manager.release());

    // Needed by note taking helper.
    arc_session_manager_ = std::make_unique<arc::ArcSessionManager>(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(&ArcSessionFactory)));
    chromeos::NoteTakingHelper::Initialize();

    AddTestUserProfile();

    lock_screen_profile_creator_ =
        std::make_unique<LockScreenProfileCreatorImpl>(primary_profile_,
                                                       &tick_clock_);
  }

  void TearDown() override {
    chromeos::NoteTakingHelper::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
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
    std::unique_ptr<base::DictionaryValue> background =
        DictionaryBuilder()
            .Set("scripts", ListBuilder().Append("background.js").Build())
            .Build();
    std::unique_ptr<base::ListValue> action_handlers =
        ListBuilder()
            .Append(DictionaryBuilder()
                        .Set("action", "new_note")
                        .Set("enabled_on_lock_screen", true)
                        .Build())
            .Build();

    DictionaryBuilder manifest_builder;
    manifest_builder.Set("name", "Note taking app")
        .Set("manifest_version", 2)
        .Set("version", "1.1")
        .Set("app", DictionaryBuilder()
                        .Set("background", std::move(background))
                        .Build())
        .Set("permissions", ListBuilder().Append("lockScreen").Build())
        .Set("action_handlers", std::move(action_handlers));

    return extensions::ExtensionBuilder()
        .SetManifest(manifest_builder.Build())
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
    chromeos::NoteTakingHelper::Get()->SetPreferredApp(profile, app->id());
    chromeos::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(
        profile, enabled);
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

  // Creates a testing primary user profile for this test.
  void AddTestUserProfile() {
    base::FilePath user_profile_path =
        user_data_dir_.GetPath().Append(ProfileHelper::Get()->GetUserProfileDir(
            ProfileHelper::GetUserIdHashByUserIdForTesting(kPrimaryUser)));

    std::unique_ptr<TestingProfile> primary_profile =
        std::make_unique<TestingProfile>(user_profile_path);
    primary_profile_ = primary_profile.get();
    profile_manager_->RegisterTestingProfile(
        std::move(primary_profile), false /*add_to_storage*/,
        false /*start_deferred_task_runner*/);
    InitExtensionSystem(primary_profile_);

    chromeos::NoteTakingHelper::Get()->SetProfileWithEnabledLockScreenApps(
        primary_profile_);
  }

  base::ScopedTempDir user_data_dir_;
  ScopedTestingLocalState local_state_;
  content::BrowserTaskEnvironment task_environment_;

  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;

  UnittestProfileManager* profile_manager_;

  TestingProfile* primary_profile_ = nullptr;

  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

  std::unique_ptr<LockScreenProfileCreator> lock_screen_profile_creator_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenProfileCreatorImplTest);
};

}  // namespace

TEST_F(LockScreenProfileCreatorImplTest,
       CreateProfileWhenLockScreenNotesEnabled) {
  EXPECT_FALSE(lock_screen_profile_creator()->Initialized());
  lock_screen_profile_creator()->Initialize();

  EXPECT_TRUE(lock_screen_profile_creator()->Initialized());

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&SetWasRun, &callback_run));

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

  // Profile should not be recreated if lock screen note taking gets reenabled.
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), false);
  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_TRUE(profile_manager()->pending_profile_creation()->path().empty());
}

TEST_F(LockScreenProfileCreatorImplTest, ProfileCreationError) {
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&SetWasRun, &callback_run));

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
      base::Bind(&SetWasRun, &callback_run));

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
      base::Bind(&SetWasRun, &callback_run));

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
      base::Bind(&SetWasRun, &first_callback_run));

  bool second_callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&SetWasRun, &second_callback_run));

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
      base::Bind(&SetWasRun, &callback_added_during_creation_run));

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(lock_screen_profile_creator()->lock_screen_profile());

  EXPECT_TRUE(first_callback_run);
  EXPECT_TRUE(second_callback_run);
  EXPECT_TRUE(callback_added_during_creation_run);

  bool callback_added_after_creation_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&SetWasRun, &callback_added_after_creation_run));
  EXPECT_TRUE(callback_added_after_creation_run);
}

TEST_F(LockScreenProfileCreatorImplTest, LockScreenProfileSetBeforeCallback) {
  lock_screen_profile_creator()->Initialize();

  Profile* lock_screen_profile = nullptr;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&LockScreenProfileCreatorImplTest::RecordLockScreenProfile,
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
      base::Bind(&LockScreenProfileCreatorImplTest::RecordLockScreenProfile,
                 base::Unretained(this), &lock_screen_profile));

  EXPECT_EQ(lock_screen_profile_creator()->lock_screen_profile(),
            lock_screen_profile);
}

TEST_F(LockScreenProfileCreatorImplTest, MetricsOnSuccess) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&SetWasRun, &callback_run));

  SetExtensionSystemReady(primary_profile());
  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());

  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(callback_run);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  tick_clock()->Advance(base::TimeDelta::FromMilliseconds(20));

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(true));

  EXPECT_TRUE(callback_run);

  histogram_tester->ExpectTimeBucketCount(
      "Apps.LockScreen.AppsProfile.Creation.Duration",
      base::TimeDelta::FromMilliseconds(20), 1);
  histogram_tester->ExpectUniqueSample(
      "Apps.LockScreen.AppsProfile.Creation.Success", 1, 1);
}

TEST_F(LockScreenProfileCreatorImplTest, MetricsOnFailure) {
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  lock_screen_profile_creator()->Initialize();

  bool callback_run = false;
  lock_screen_profile_creator()->AddCreateProfileCallback(
      base::Bind(&SetWasRun, &callback_run));

  SetExtensionSystemReady(primary_profile());
  scoped_refptr<const extensions::Extension> test_app =
      AddTestNoteTakingApp(primary_profile());

  SetAppEnabledOnLockScreen(primary_profile(), test_app.get(), true);

  EXPECT_FALSE(lock_screen_profile_creator()->ProfileCreated());
  EXPECT_FALSE(callback_run);

  ASSERT_EQ(ProfileHelper::GetLockScreenAppProfilePath(),
            profile_manager()->pending_profile_creation()->path());

  tick_clock()->Advance(base::TimeDelta::FromMilliseconds(20));

  ASSERT_TRUE(profile_manager()
                  ->pending_profile_creation()
                  ->WaitForCreationAndOverrideResponse(false));

  EXPECT_TRUE(callback_run);

  histogram_tester->ExpectTotalCount(
      "Apps.LockScreen.AppsProfile.Creation.Duration", 0);
  histogram_tester->ExpectUniqueSample(
      "Apps.LockScreen.AppsProfile.Creation.Success", 0, 1);
}

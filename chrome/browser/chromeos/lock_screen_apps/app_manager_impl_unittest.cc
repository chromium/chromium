// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/app_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/fake_lock_screen_profile_creator.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_session.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::DictionaryBuilder;
using extensions::ListBuilder;

namespace lock_screen_apps {

namespace {

constexpr int kMaxLockScreenAppReloadsCount = 3;

std::unique_ptr<arc::ArcSession> ArcSessionFactory() {
  ADD_FAILURE() << "Attempt to create arc session.";
  return nullptr;
}

class LockScreenEventRouter : public extensions::TestEventRouter {
 public:
  explicit LockScreenEventRouter(content::BrowserContext* context)
      : extensions::TestEventRouter(context) {}
  ~LockScreenEventRouter() override = default;

  // extensions::EventRouter:
  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name) const override {
    return event_name == extensions::api::app_runtime::OnLaunched::kEventName;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LockScreenEventRouter);
};

class LockScreenEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  explicit LockScreenEventObserver(content::BrowserContext* context)
      : context_(context) {}
  ~LockScreenEventObserver() override = default;

  // extensions::TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const extensions::Event& event) override {
    if (event.event_name !=
        extensions::api::app_runtime::OnLaunched::kEventName) {
      return;
    }
    ASSERT_TRUE(event.event_args);
    const base::Value* arg_value = nullptr;
    ASSERT_TRUE(event.event_args->Get(0, &arg_value));
    ASSERT_TRUE(arg_value);
    if (event.restrict_to_browser_context)
      EXPECT_EQ(context_, event.restrict_to_browser_context);

    std::unique_ptr<extensions::api::app_runtime::LaunchData> launch_data =
        extensions::api::app_runtime::LaunchData::FromValue(*arg_value);
    ASSERT_TRUE(launch_data);
    ASSERT_TRUE(launch_data->action_data);
    EXPECT_EQ(extensions::api::app_runtime::ACTION_TYPE_NEW_NOTE,
              launch_data->action_data->action_type);

    ASSERT_TRUE(launch_data->action_data->is_lock_screen_action);
    EXPECT_TRUE(*launch_data->action_data->is_lock_screen_action);

    ASSERT_TRUE(launch_data->action_data->restore_last_action_state);
    EXPECT_EQ(expect_restore_action_state_,
              *launch_data->action_data->restore_last_action_state);

    launched_apps_.push_back(extension_id);
  }

  const std::vector<std::string>& launched_apps() const {
    return launched_apps_;
  }

  void ClearLaunchedApps() { launched_apps_.clear(); }

  void set_expect_restore_action_state(bool expect_restore_action_state) {
    expect_restore_action_state_ = expect_restore_action_state;
  }

 private:
  std::vector<std::string> launched_apps_;
  content::BrowserContext* context_;
  bool expect_restore_action_state_ = true;

  DISALLOW_COPY_AND_ASSIGN(LockScreenEventObserver);
};

enum class TestAppLocation { kUnpacked, kInternal };

class LockScreenAppManagerImplTest
    : public testing::TestWithParam<TestAppLocation> {
 public:
  LockScreenAppManagerImplTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ~LockScreenAppManagerImplTest() override = default;

  void SetUp() override {
    // Initialize command line so chromeos::NoteTakingHelper thinks note taking
    // on lock screen is enabled.
    command_line_ = std::make_unique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv(
        {"", "--enable-lock-screen-apps", "--force-enable-stylus-tools"});

    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = profile_manager_.CreateTestingProfile("primary_profile");

    InitExtensionSystem(profile());

    // Initialize arc session manager - NoteTakingHelper expects it to be set.
    arc_session_manager_ = std::make_unique<arc::ArcSessionManager>(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(&ArcSessionFactory)));

    chromeos::NoteTakingHelper::Initialize();
    chromeos::NoteTakingHelper::Get()->SetProfileWithEnabledLockScreenApps(
        profile());

    lock_screen_profile_creator_ =
        std::make_unique<lock_screen_apps::FakeLockScreenProfileCreator>(
            &profile_manager_);
    lock_screen_profile_creator_->Initialize();

    ResetAppManager();
  }

  void TearDown() override {
    // App manager has to be destroyed before NoteTakingHelper is shutdown - it
    // removes itself from the NoteTakingHelper observer list during its
    // destruction.
    app_manager_.reset();

    chromeos::NoteTakingHelper::Shutdown();
    extensions::ExtensionSystem::Get(profile())->Shutdown();
  }

  void InitExtensionSystem(Profile* profile) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        profile->GetPath().Append("Extensions") /* install_directory */,
        false /* autoupdate_enabled */);
  }

  void SetUpTestEventRouter() {
    LockScreenEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter<LockScreenEventRouter>(
            LockScreenProfile()->GetOriginalProfile());
    event_observer_ = std::make_unique<LockScreenEventObserver>(
        LockScreenProfile()->GetOriginalProfile());
    event_router->AddEventObserver(event_observer_.get());
  }

  base::FilePath GetTestAppSourcePath(TestAppLocation location,
                                      Profile* profile,
                                      const std::string& id,
                                      const std::string& version) {
    switch (location) {
      case TestAppLocation::kUnpacked:
        return profile->GetPath().Append("Downloads").Append("app");
      case TestAppLocation::kInternal:
        return extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->install_directory()
            .Append(id)
            .Append(version);
    }
    return base::FilePath();
  }

  base::FilePath GetLockScreenAppPath(const std::string& id,
                                      const std::string& version) {
    return GetLockScreenAppPathWithOriginalProfile(profile(), id, version);
  }

  base::FilePath GetLockScreenAppPathWithOriginalProfile(
      Profile* original_profile,
      const std::string& id,
      const std::string& version) {
    return GetLockScreenAppPathWithOriginalLocation(
        GetParam(), original_profile, id, version);
  }

  base::FilePath GetLockScreenAppPathWithOriginalLocation(
      TestAppLocation location,
      Profile* original_profile,
      const std::string& id,
      const std::string& version) {
    switch (location) {
      case TestAppLocation::kUnpacked:
        return original_profile->GetPath().Append("Downloads").Append("app");
      case TestAppLocation::kInternal:
        return extensions::ExtensionSystem::Get(LockScreenProfile())
            ->extension_service()
            ->install_directory()
            .Append(id)
            .Append(version + "_0");
    }
    return base::FilePath();
  }

  extensions::Manifest::Location GetAppLocation(TestAppLocation location) {
    switch (location) {
      case TestAppLocation::kUnpacked:
        return extensions::Manifest::UNPACKED;
      case TestAppLocation::kInternal:
        return extensions::Manifest::INTERNAL;
    }

    return extensions::Manifest::UNPACKED;
  }

  scoped_refptr<const extensions::Extension> CreateTestApp(
      const std::string& id,
      const std::string& version,
      bool supports_lock_screen) {
    return CreateTestAppInProfile(profile(), id, version, supports_lock_screen);
  }

  scoped_refptr<const extensions::Extension> CreateTestAppInProfile(
      Profile* profile,
      const std::string& id,
      const std::string& version,
      bool supports_lock_screen) {
    return CreateTestAppWithLocation(GetParam(), profile, id, version,
                                     supports_lock_screen);
  }

  scoped_refptr<const extensions::Extension> CreateTestAppWithLocation(
      TestAppLocation location,
      Profile* profile,
      const std::string& id,
      const std::string& version,
      bool supports_lock_screen) {
    std::unique_ptr<base::DictionaryValue> background =
        DictionaryBuilder()
            .Set("scripts", ListBuilder().Append("background.js").Build())
            .Build();
    std::unique_ptr<base::ListValue> action_handlers =
        ListBuilder()
            .Append(DictionaryBuilder()
                        .Set("action", "new_note")
                        .Set("enabled_on_lock_screen", supports_lock_screen)
                        .Build())
            .Build();

    DictionaryBuilder manifest_builder;
    manifest_builder.Set("name", "Note taking app")
        .Set("version", version)
        .Set("manifest_version", 2)
        .Set("app", DictionaryBuilder()
                        .Set("background", std::move(background))
                        .Build())
        .Set("permissions", ListBuilder().Append("lockScreen").Build())
        .Set("action_handlers", std::move(action_handlers));

    base::FilePath extension_path =
        GetTestAppSourcePath(location, profile, id, version);

    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(manifest_builder.Build())
            .SetID(id)
            .SetPath(extension_path)
            .SetLocation(GetAppLocation(location))
            .Build();

    // Create the app path with required files - app manager *will* attempt to
    // load the app from the disk, so extension directory has to be present for
    // the load to succeed.
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(extension_path, &error)) {
      ADD_FAILURE() << "Failed to create path " << extension_path.value() << " "
                    << error;
      return nullptr;
    }

    JSONFileValueSerializer manifest_writer(
        extension_path.Append("manifest.json"));
    if (!manifest_writer.Serialize(*extension->manifest()->value())) {
      ADD_FAILURE() << "Failed to create manifest file";
      return nullptr;
    }

    if (base::WriteFile(extension_path.Append("background.js"), "{}", 2) != 2) {
      ADD_FAILURE() << "Failed to write background script file";
      return nullptr;
    }

    return extension;
  }

  TestingProfile* CreateSecondaryProfile() {
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile("secondary_profile");
    InitExtensionSystem(profile);
    return profile;
  }

  scoped_refptr<const extensions::Extension> AddTestAppWithLockScreenSupport(
      Profile* profile,
      const std::string& app_id,
      const std::string& version,
      bool enable_on_lock_screen) {
    scoped_refptr<const extensions::Extension> app = CreateTestAppInProfile(
        profile, app_id, version, true /* supports_lock_screen*/);
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->AddExtension(app.get());

    chromeos::NoteTakingHelper::Get()->SetPreferredApp(profile, app_id);
    chromeos::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(
        profile, enable_on_lock_screen);
    return app;
  }

  // Initializes and starts app manager.
  // |create_lock_screen_profile| - whether the lock screen profile should be
  //     created (using the lock screen profile creator passed to the app
  //     manager) before starting the app manager. If this is not set, the test
  //     should create the lock screen profile itself (and should not use
  //     |LockScreenProfile| before that).
  void InitializeAndStartAppManager(Profile* profile,
                                    bool create_lock_screen_profile) {
    app_manager()->Initialize(profile, lock_screen_profile_creator_.get());
    if (create_lock_screen_profile)
      CreateLockScreenProfile();
    app_manager()->Start(
        base::Bind(&LockScreenAppManagerImplTest::OnNoteTakingChanged,
                   base::Unretained(this)));
  }

  void RestartLockScreenAppManager() {
    app_manager()->Stop();
    app_manager()->Start(
        base::Bind(&LockScreenAppManagerImplTest::OnNoteTakingChanged,
                   base::Unretained(this)));
  }

  void CreateLockScreenProfile() {
    lock_screen_profile_creator_->CreateProfile();
    if (needs_lock_screen_event_router_)
      SetUpTestEventRouter();
  }

  void set_needs_lock_screen_event_router() {
    needs_lock_screen_event_router_ = true;
  }

  TestingProfile* profile() { return profile_; }

  Profile* LockScreenProfile() {
    return lock_screen_profile_creator_->lock_screen_profile();
  }

  AppManager* app_manager() { return app_manager_.get(); }

  void ResetAppManager() {
    app_manager_ = std::make_unique<AppManagerImpl>(&tick_clock_);
  }

  int note_taking_changed_count() const { return note_taking_changed_count_; }

  void ResetNoteTakingChangedCount() { note_taking_changed_count_ = 0; }

  // Waits for a round trip between file task runner used by the profile's
  // extension service and the main thread - used to ensure that all pending
  // file runner task finish,
  void RunExtensionServiceTaskRunner(Profile* profile) {
    base::RunLoop run_loop;
    extensions::GetExtensionFileTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

  bool IsInstallAsync() { return GetParam() != TestAppLocation::kUnpacked; }

  int NoteTakingChangedCountOnStart() { return IsInstallAsync() ? 1 : 0; }

  LockScreenEventObserver* event_observer() { return event_observer_.get(); }

  FakeLockScreenProfileCreator* lock_screen_profile_creator() {
    return lock_screen_profile_creator_.get();
  }

 protected:
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<lock_screen_apps::FakeLockScreenProfileCreator>
      lock_screen_profile_creator_;

 private:
  void OnNoteTakingChanged() { ++note_taking_changed_count_; }

  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  content::BrowserTaskEnvironment task_environment_;

  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager user_manager_;

  TestingProfileManager profile_manager_;
  TestingProfile* profile_ = nullptr;

  std::unique_ptr<LockScreenEventObserver> event_observer_;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

  std::unique_ptr<AppManager> app_manager_;

  bool needs_lock_screen_event_router_ = false;
  int note_taking_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppManagerImplTest);
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(Unpacked,
                         LockScreenAppManagerImplTest,
                         ::testing::Values(TestAppLocation::kUnpacked));
INSTANTIATE_TEST_SUITE_P(Internal,
                         LockScreenAppManagerImplTest,
                         ::testing::Values(TestAppLocation::kInternal));

TEST_P(LockScreenAppManagerImplTest, StartAddsAppToTarget) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  EXPECT_TRUE(base::PathExists(note_taking_app->path()));

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(lock_app);

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, StartWhenLockScreenNotesNotEnabled) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          false /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  EXPECT_FALSE(lock_app);

  app_manager()->Stop();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(lock_app);

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingDisabledWhileStarted) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());
  EXPECT_TRUE(base::PathExists(note_taking_app->path()));

  chromeos::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(
      profile(), false);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());
  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(lock_app);

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingEnabledWhileStarted) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          false /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(lock_app);

  chromeos::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(
      profile(), true);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());
  EXPECT_TRUE(base::PathExists(note_taking_app->path()));

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingChangedWhileStarted) {
  scoped_refptr<const extensions::Extension> dev_note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  scoped_refptr<const extensions::Extension> prod_note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(prod_note_taking_app->id(),
                                 prod_note_taking_app->VersionString()),
            lock_app->path());
  EXPECT_TRUE(base::PathExists(prod_note_taking_app->path()));

  chromeos::NoteTakingHelper::Get()->SetPreferredApp(
      profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  // Verify prod app was unloaded from signin profile.
  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(lock_app);

  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kDevKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);

  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(dev_note_taking_app->id(),
                                 dev_note_taking_app->VersionString()),
            lock_app->path());

  app_manager()->Stop();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(dev_note_taking_app->path()));
  EXPECT_TRUE(base::PathExists(prod_note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, NoteTakingChangedToLockScreenSupported) {
  scoped_refptr<const extensions::Extension> dev_note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  scoped_refptr<const extensions::Extension> prod_note_taking_app =
      CreateTestAppInProfile(profile(),
                             chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             "1.0", false /* supports_lock_screen */);
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->AddExtension(prod_note_taking_app.get());
  chromeos::NoteTakingHelper::Get()->SetPreferredApp(
      profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId);

  // Initialize app manager - the note taking should be disabled initially
  // because the preferred app (prod) is not enabled on lock screen.
  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(false, app_manager()->IsNoteTakingAppAvailable());

  // Setting dev app, which is enabled on lock screen, as preferred will enable
  // lock screen note taking,
  chromeos::NoteTakingHelper::Get()->SetPreferredApp(
      profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  // If test app is installed asynchronously. the app won't be enabled on
  // lock screen until extension service task runner tasks are run.
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  // Verify the dev app copy is installed in the lock screen app profile.
  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kDevKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);
  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(dev_note_taking_app->id(),
                                 dev_note_taking_app->VersionString()),
            lock_app->path());

  // Verify the prod app was not coppied to the lock screen profile (for
  // unpacked apps, the lock screen extension path is the same as the original
  // app path, so it's expected to still exist).
  EXPECT_EQ(
      GetParam() == TestAppLocation::kUnpacked,
      base::PathExists(GetLockScreenAppPath(
          prod_note_taking_app->id(), prod_note_taking_app->VersionString())));

  app_manager()->Stop();

  // Stopping app manager will disable lock screen note taking.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  // Make sure original app paths are not deleted.
  EXPECT_TRUE(base::PathExists(dev_note_taking_app->path()));
  EXPECT_TRUE(base::PathExists(prod_note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingReloadedWhileStarted) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);
  EXPECT_EQ("1.0", lock_app->VersionString());

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());
  EXPECT_TRUE(base::PathExists(note_taking_app->path()));

  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->UnloadExtension(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                        extensions::UnloadedExtensionReason::UPDATE);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  // Verify prod app was unloaded from signin profile.
  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(lock_app);

  // Add the app again.
  note_taking_app = CreateTestApp(
      chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.1", true);
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->AddExtension(note_taking_app.get());

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);

  ASSERT_TRUE(lock_app);
  EXPECT_EQ("1.1", lock_app->VersionString());

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());

  app_manager()->Stop();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest,
       NoteTakingAppChangeToUnpackedWhileActivating) {
  scoped_refptr<const extensions::Extension> initial_note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.1",
          true /* enable_on_lock_screen */);

  scoped_refptr<const extensions::Extension> final_note_taking_app =
      CreateTestAppWithLocation(TestAppLocation::kUnpacked, profile(),
                                chromeos::NoteTakingHelper::kDevKeepExtensionId,
                                "1.1", true /* enable_on_lock_screen */);
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->AddExtension(final_note_taking_app.get());

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  chromeos::NoteTakingHelper::Get()->SetPreferredApp(
      profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId);

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kDevKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);
  EXPECT_EQ("1.1", lock_app->VersionString());

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(
      GetLockScreenAppPathWithOriginalLocation(
          TestAppLocation::kUnpacked, profile(), final_note_taking_app->id(),
          final_note_taking_app->VersionString()),
      lock_app->path());

  app_manager()->Stop();

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(initial_note_taking_app->path()));
  EXPECT_TRUE(base::PathExists(final_note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest,
       NoteTakingAppChangeToInternalWhileActivating) {
  scoped_refptr<const extensions::Extension> initial_note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.1",
          true /* enable_on_lock_screen */);

  scoped_refptr<const extensions::Extension> final_note_taking_app =
      CreateTestAppWithLocation(TestAppLocation::kInternal, profile(),
                                chromeos::NoteTakingHelper::kDevKeepExtensionId,
                                "1.1", true /* enable_on_lock_screen */);
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->AddExtension(final_note_taking_app.get());

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  chromeos::NoteTakingHelper::Get()->SetPreferredApp(
      profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId);

  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kDevKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);
  EXPECT_EQ("1.1", lock_app->VersionString());

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(
      GetLockScreenAppPathWithOriginalLocation(
          TestAppLocation::kInternal, profile(), final_note_taking_app->id(),
          final_note_taking_app->VersionString()),
      lock_app->path());

  app_manager()->Stop();

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(base::PathExists(initial_note_taking_app->path()));
  EXPECT_TRUE(base::PathExists(final_note_taking_app->path()));
}

TEST_P(LockScreenAppManagerImplTest, ShutdownWhenStarted) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.1",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kProdKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  EXPECT_TRUE(lock_app);
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWhenEnabled) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  ASSERT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  EXPECT_TRUE(app_manager()->LaunchNoteTaking());

  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            event_observer()->launched_apps()[0]);
  event_observer()->ClearLaunchedApps();

  app_manager()->Stop();

  EXPECT_FALSE(app_manager()->LaunchNoteTaking());
  EXPECT_TRUE(event_observer()->launched_apps().empty());
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWithFalseRestoreLastActionState) {
  set_needs_lock_screen_event_router();

  profile()->GetPrefs()->SetBoolean(prefs::kRestoreLastLockScreenNote, false);

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  ASSERT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  event_observer()->set_expect_restore_action_state(false);
  EXPECT_TRUE(app_manager()->LaunchNoteTaking());

  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(chromeos::NoteTakingHelper::kProdKeepExtensionId,
            event_observer()->launched_apps()[0]);
  event_observer()->ClearLaunchedApps();

  app_manager()->Stop();

  EXPECT_FALSE(app_manager()->LaunchNoteTaking());
  EXPECT_TRUE(event_observer()->launched_apps().empty());
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWhenNoLockScreenApp) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          false /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_FALSE(app_manager()->LaunchNoteTaking());
  EXPECT_TRUE(event_observer()->launched_apps().empty());

  app_manager()->Stop();
  EXPECT_FALSE(app_manager()->LaunchNoteTaking());
  EXPECT_TRUE(event_observer()->launched_apps().empty());
}

TEST_P(LockScreenAppManagerImplTest, InitializedAfterLockScreenProfileCreated) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  CreateLockScreenProfile();

  InitializeAndStartAppManager(profile(), false /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kDevKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());
  EXPECT_TRUE(base::PathExists(note_taking_app->path()));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest, StartedBeforeLockScreenProfileCreated) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), false /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  CreateLockScreenProfile();

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsNoteTakingAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(chromeos::NoteTakingHelper::kDevKeepExtensionId,
            app_manager()->GetNoteTakingAppId());

  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(chromeos::NoteTakingHelper::kDevKeepExtensionId,
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);

  EXPECT_TRUE(base::PathExists(lock_app->path()));
  EXPECT_EQ(GetLockScreenAppPath(note_taking_app->id(),
                                 note_taking_app->VersionString()),
            lock_app->path());
  EXPECT_TRUE(base::PathExists(note_taking_app->path()));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest, LockScreenProfileCreatedNoSupportedApp) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          false /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), false /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  CreateLockScreenProfile();
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest, LockScreenProfileCreationFailure) {
  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), false /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  lock_screen_profile_creator()->SetProfileCreationFailed();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());
}

TEST_P(LockScreenAppManagerImplTest,
       LockScreenProfileCreationFailedBeforeInitialization) {
  lock_screen_profile_creator()->SetProfileCreationFailed();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kDevKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), false /*create_lock_screen_profile*/);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());
}

TEST_P(LockScreenAppManagerImplTest, ReloadLockScreenAppAfterAppCrash) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Simulate lock screen note app crash.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->TerminateExtension(note_taking_app->id());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(note_taking_app->id(), app_manager()->GetNoteTakingAppId());

  // App launch should be successful - this action should reload the
  // terminated app.
  EXPECT_TRUE(app_manager()->LaunchNoteTaking());

  // Verify the lock screen note app is enabled.
  const extensions::Extension* lock_app =
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(note_taking_app->id(),
                             extensions::ExtensionRegistry::ENABLED);
  ASSERT_TRUE(lock_app);
  EXPECT_EQ("1.0", lock_app->VersionString());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(lock_app->id(), event_observer()->launched_apps()[0]);
  event_observer()->ClearLaunchedApps();
}

TEST_P(LockScreenAppManagerImplTest, AppReloadFailure) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Simulate lock screen note app crash.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->TerminateExtension(note_taking_app->id());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(note_taking_app->id(), app_manager()->GetNoteTakingAppId());

  // Disable the note taking app in the lock screen app profile - this should
  // prevent app reload.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->DisableExtension(note_taking_app->id(),
                         extensions::disable_reason::DISABLE_USER_ACTION);

  // App launch should fail - given that the app got disabled, it should not
  // be reloadable anymore.
  EXPECT_FALSE(app_manager()->LaunchNoteTaking());

  // Make sure that note taking is not reported as available any longer.
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, LockScreenAppGetsUninstalled) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Disable the note taking app in the lock screen app profile.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->UninstallExtension(note_taking_app->id(),
                           extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  // Note taking should be reported to be unavailable if the app was uninstalled
  // from the lock screen profile.
  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, TerminatedAppGetsUninstalled) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Simulate lock screen note app crash.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->TerminateExtension(note_taking_app->id());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(note_taking_app->id(), app_manager()->GetNoteTakingAppId());

  // Disable the note taking app in the lock screen app profile - this should
  // prevent app reload.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->UninstallExtension(note_taking_app->id(),
                           extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  // Note taking should be reported to be unavailable if the app was uninstalled
  // from the lock screen profile.
  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, DoNotReloadLockScreenAppWhenDisabled) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Disable the lock screen app..
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->DisableExtension(note_taking_app->id(),
                         extensions::disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());
  EXPECT_FALSE(app_manager()->LaunchNoteTaking());
  EXPECT_FALSE(
      extensions::ExtensionRegistry::Get(LockScreenProfile())
          ->GetExtensionById(note_taking_app->id(),
                             extensions::ExtensionRegistry::EVERYTHING));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest,
       RestartingAppManagerAfterLockScreenAppDisabled) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Disable the lock screen app..
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->DisableExtension(note_taking_app->id(),
                         extensions::disable_reason::DISABLE_USER_ACTION);

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());

  // Restarting the app manager should enable lock screen app again.
  RestartLockScreenAppManager();
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(note_taking_app->id(), app_manager()->GetNoteTakingAppId());
  EXPECT_TRUE(app_manager()->LaunchNoteTaking());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(note_taking_app->id(), event_observer()->launched_apps()[0]);
}

TEST_P(LockScreenAppManagerImplTest, AppNotReloadedAfterRepeatedCrashes) {
  set_needs_lock_screen_event_router();

  scoped_refptr<const extensions::Extension> note_taking_app =
      AddTestAppWithLockScreenSupport(
          profile(), chromeos::NoteTakingHelper::kProdKeepExtensionId, "1.0",
          true /* enable_on_lock_screen */);

  InitializeAndStartAppManager(profile(), true /*create_lock_screen_profile*/);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Simulate lock screen note app crash and launch few times.
  for (int i = 0; i < kMaxLockScreenAppReloadsCount; ++i) {
    extensions::ExtensionSystem::Get(LockScreenProfile())
        ->extension_service()
        ->TerminateExtension(note_taking_app->id());
    EXPECT_TRUE(app_manager()->LaunchNoteTaking());
  }

  // If app is reloaded too many times, lock screen app should eventually
  // become unavailable.
  extensions::ExtensionSystem::Get(LockScreenProfile())
      ->extension_service()
      ->TerminateExtension(note_taking_app->id());

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_TRUE(app_manager()->GetNoteTakingAppId().empty());
  EXPECT_FALSE(app_manager()->LaunchNoteTaking());
  EXPECT_FALSE(extensions::ExtensionRegistry::Get(LockScreenProfile())
                   ->GetExtensionById(note_taking_app->id(),
                                      extensions::ExtensionRegistry::ENABLED));
  event_observer()->ClearLaunchedApps();

  // Restarting the app manager should enable lock screen app again.
  RestartLockScreenAppManager();
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_TRUE(app_manager()->IsNoteTakingAppAvailable());
  EXPECT_EQ(note_taking_app->id(), app_manager()->GetNoteTakingAppId());
  EXPECT_TRUE(app_manager()->LaunchNoteTaking());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(note_taking_app->id(), event_observer()->launched_apps()[0]);
}

}  // namespace lock_screen_apps

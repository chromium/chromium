// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/app_manager_impl.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/traits_bag.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/lock_screen_apps/fake_lock_screen_profile_creator.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  LockScreenEventRouter(const LockScreenEventRouter&) = delete;
  LockScreenEventRouter& operator=(const LockScreenEventRouter&) = delete;

  ~LockScreenEventRouter() override = default;

  // extensions::EventRouter:
  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name) const override {
    return event_name == extensions::api::app_runtime::OnLaunched::kEventName;
  }
};

class LockScreenEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  explicit LockScreenEventObserver(content::BrowserContext* context)
      : context_(context) {}

  LockScreenEventObserver(const LockScreenEventObserver&) = delete;
  LockScreenEventObserver& operator=(const LockScreenEventObserver&) = delete;

  ~LockScreenEventObserver() override = default;

  // extensions::TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const extensions::Event& event) override {
    if (event.event_name !=
        extensions::api::app_runtime::OnLaunched::kEventName) {
      return;
    }
    const base::Value& arg_value = event.event_args[0];
    if (event.restrict_to_browser_context)
      EXPECT_EQ(context_, event.restrict_to_browser_context);

    ASSERT_TRUE(arg_value.is_dict());
    std::optional<extensions::api::app_runtime::LaunchData> launch_data =
        extensions::api::app_runtime::LaunchData::FromValue(
            arg_value.GetDict());
    ASSERT_TRUE(launch_data->action_data);
    EXPECT_EQ(extensions::api::app_runtime::ActionType::kNewNote,
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
  raw_ptr<content::BrowserContext> context_;
  bool expect_restore_action_state_ = true;
};

enum class TestAppType { kUnpackedChromeApp, kInternalChromeApp };

struct TestApp {
  const char* extension_id = "";
  const char* version = "";
  bool supports_lock_screen = false;
};

// TODO (crbug.com/1332379): Stop using real extension IDs here.
// A lock screen capable app.
const TestApp kLockScreenCapableApp{
    .extension_id = ash::NoteTakingHelper::kProdKeepExtensionId,
    .version = "1.0",
    .supports_lock_screen = true};
// An updated version of `kLockScreenCapable` (same ID).
const TestApp kLockScreenCapableAppUpdated{
    .extension_id = ash::NoteTakingHelper::kProdKeepExtensionId,
    .version = "1.1",
    .supports_lock_screen = true};
// Another lock screen capable app (different ID from `kLockScreenCapable`).
const TestApp kLockScreenCapableApp2{
    .extension_id = ash::NoteTakingHelper::kDevKeepExtensionId,
    .version = "1.0",
    .supports_lock_screen = true};
// A note-taking app that is not lock screen capable.
const TestApp kNotLockScreenCapableApp{
    .extension_id = ash::NoteTakingHelper::kNoteTakingWebAppIdTest,
    .version = "1.0",
    .supports_lock_screen = false};

class LockScreenAppManagerImplTest
    : public testing::TestWithParam<TestAppType> {
 public:
  LockScreenAppManagerImplTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  LockScreenAppManagerImplTest(const LockScreenAppManagerImplTest&) = delete;
  LockScreenAppManagerImplTest& operator=(const LockScreenAppManagerImplTest&) =
      delete;

  ~LockScreenAppManagerImplTest() override = default;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = CreatePrimaryProfile();

    InitExtensionSystem(profile());

    // Wait for AppServiceProxy to be ready - NoteTakingHelper depends on
    // AppService.
    WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(profile_));

    // Initialize arc session manager - NoteTakingHelper expects it to be set.
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(&ArcSessionFactory)));

    ash::NoteTakingHelper::Initialize();

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

    lock_screen_profile_creator_.reset();
    ash::NoteTakingHelper::Shutdown();
    arc_session_manager_.reset();
    extensions::ExtensionSystem::Get(profile())->Shutdown();

    ash::ConciergeClient::Shutdown();
  }

  void InitExtensionSystem(Profile* profile) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        /*install_directory=*/profile->GetPath().Append("Extensions"),
        /*autoupdate_enabled=*/false);
  }

  void SetUpTestEventRouter() {
    LockScreenEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter<LockScreenEventRouter>(
            LockScreenProfile()->GetOriginalProfile());
    event_observer_ = std::make_unique<LockScreenEventObserver>(
        LockScreenProfile()->GetOriginalProfile());
    event_router->AddEventObserver(event_observer_.get());
  }

  base::FilePath GetTestAppSourcePath(TestAppType appType,
                                      Profile* profile,
                                      const std::string& id,
                                      const std::string& version) {
    switch (appType) {
      case TestAppType::kUnpackedChromeApp:
        return profile->GetPath().Append("Downloads").Append("app");
      case TestAppType::kInternalChromeApp:
        return extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->install_directory()
            .Append(id)
            .Append(version);
    }
  }

  base::FilePath GetExpectedLockScreenAppPath(const TestApp& test_app) {
    return GetExpectedLockScreenAppPathForAppType(GetParam(), profile(),
                                                  test_app);
  }

  base::FilePath GetExpectedLockScreenAppPathForAppType(
      TestAppType appType,
      Profile* original_profile,
      const TestApp& test_app) {
    switch (appType) {
      case TestAppType::kUnpackedChromeApp:
        return original_profile->GetPath().Append("Downloads").Append("app");
      case TestAppType::kInternalChromeApp:
        return extensions::ExtensionSystem::Get(LockScreenProfile())
            ->extension_service()
            ->install_directory()
            .Append(test_app.extension_id)
            .Append(std::string(test_app.version) + "_0");
    }
  }

  extensions::mojom::ManifestLocation GetAppLocation(TestAppType appType) {
    switch (appType) {
      case TestAppType::kUnpackedChromeApp:
        return extensions::mojom::ManifestLocation::kUnpacked;
      case TestAppType::kInternalChromeApp:
        return extensions::mojom::ManifestLocation::kInternal;
    }
  }

  // Returns the ID of installed app.
  std::string InstallTestApp(const TestApp& test_app) {
    return InstallTestAppWithType(GetParam(), profile(), test_app);
  }

  // Returns the ID of installed app.
  std::string InstallTestAppWithType(TestAppType type,
                                     Profile* profile,
                                     const TestApp& test_app) {
    scoped_refptr<const extensions::Extension> extension =
        MakeChromeApp(type, profile, test_app);
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->AddExtension(extension.get());
    return extension->id();
  }

  scoped_refptr<const extensions::Extension> MakeChromeApp(
      TestAppType appType,
      Profile* profile,
      const TestApp& test_app) {
    std::string id = test_app.extension_id;
    std::string version = test_app.version;
    bool supports_lock_screen = test_app.supports_lock_screen;

    base::Value::Dict background = base::Value::Dict().Set(
        "scripts", base::Value::List().Append("background.js"));
    base::Value::List action_handlers = base::Value::List().Append(
        base::Value::Dict()
            .Set("action", "new_note")
            .Set("enabled_on_lock_screen", supports_lock_screen));

    auto manifest_builder =
        base::Value::Dict()
            .Set("name", "Note taking app")
            .Set("version", version)
            .Set("manifest_version", 2)
            .Set("app",
                 base::Value::Dict().Set("background", std::move(background)))
            .Set("permissions", base::Value::List().Append("lockScreen"))
            .Set("action_handlers", std::move(action_handlers));

    base::FilePath extension_path =
        GetTestAppSourcePath(appType, profile, id, version);

    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(std::move(manifest_builder))
            .SetID(id)
            .SetPath(extension_path)
            .SetLocation(GetAppLocation(appType))
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

    if (!base::WriteFile(extension_path.Append("background.js"), "{}")) {
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

  // Returns app ID.
  std::string AddTestAppWithLockScreenSupport(const TestApp& test_app,
                                              bool enable_on_lock_screen) {
    DCHECK(test_app.supports_lock_screen);
    std::string app_id = InstallTestApp(test_app);

    ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id);
    ash::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(
        profile(), enable_on_lock_screen);
    return app_id;
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
        base::BindRepeating(&LockScreenAppManagerImplTest::OnNoteTakingChanged,
                            base::Unretained(this)));
  }

  void RestartLockScreenAppManager() {
    app_manager()->Stop();
    app_manager()->Start(
        base::BindRepeating(&LockScreenAppManagerImplTest::OnNoteTakingChanged,
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

  bool IsInstallAsync() {
    return GetParam() != TestAppType::kUnpackedChromeApp;
  }

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

  TestingProfile* CreatePrimaryProfile() {
    DCHECK(!scoped_user_manager_) << "there can be only one primary profile";
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id(AccountId::FromUserEmail("primary_profile"));
    user_manager->AddPublicAccountUser(account_id);
    user_manager->LoginUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    return profile_manager_.CreateTestingProfile("primary_profile");
  }

  content::BrowserTaskEnvironment task_environment_;

  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;

  std::unique_ptr<LockScreenEventObserver> event_observer_;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

  std::unique_ptr<AppManager> app_manager_;

  bool needs_lock_screen_event_router_ = false;
  int note_taking_changed_count_ = 0;
};

bool IsInstalled(const std::string& app_id, Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING);
  return app;
}

bool IsInstalledAndEnabled(const std::string& app_id, Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          app_id);
  return app;
}

bool PathExists(const std::string& app_id, Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          app_id);
  return app && base::PathExists(app->path());
}

base::FilePath GetPath(const std::string& app_id, Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          app_id);
  return app ? app->path() : base::FilePath();
}

std::optional<std::string> GetVersion(const std::string& app_id,
                                      Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          app_id);
  if (!app)
    return std::nullopt;
  return app->VersionString();
}

void UnloadApp(const std::string& app_id, Profile* profile) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UnloadExtension(app_id, extensions::UnloadedExtensionReason::UPDATE);
}

void SimulateAppCrash(const std::string& app_id, Profile* profile) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->TerminateExtension(app_id);
}

void DisableApp(const std::string& app_id, Profile* profile) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->DisableExtension(app_id,
                         extensions::disable_reason::DISABLE_USER_ACTION);
}

void UninstallApp(const std::string& app_id, Profile* profile) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UninstallExtension(app_id, extensions::UNINSTALL_REASON_FOR_TESTING,
                           nullptr);
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(Unpacked,
                         LockScreenAppManagerImplTest,
                         ::testing::Values(TestAppType::kUnpackedChromeApp));
INSTANTIATE_TEST_SUITE_P(Internal,
                         LockScreenAppManagerImplTest,
                         ::testing::Values(TestAppType::kInternalChromeApp));

TEST_P(LockScreenAppManagerImplTest, StartAddsAppToTarget) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, profile()));

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, StartWhenLockScreenNotesNotEnabled) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/false);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  app_manager()->Stop();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingDisabledWhileStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());
  EXPECT_TRUE(PathExists(app_id, profile()));

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));

  ash::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(profile(),
                                                                   false);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingEnabledWhileStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/false);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  ash::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(profile(),
                                                                   true);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, profile()));

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingChangedWhileStarted) {
  std::string app_id_1 = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  std::string app_id_2 = InstallTestApp(kLockScreenCapableApp2);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id_1, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id_1, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id_1, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id_1, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id_1, profile()));

  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id_2);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id_2, app_manager()->GetLockScreenAppId());

  // Verify first app was unloaded from lock screen app profile.
  EXPECT_FALSE(IsInstalled(app_id_1, LockScreenProfile()));

  ASSERT_TRUE(IsInstalledAndEnabled(app_id_2, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id_2, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp2),
            GetPath(app_id_2, LockScreenProfile()));

  app_manager()->Stop();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id_2, profile()));
  EXPECT_TRUE(PathExists(app_id_1, profile()));
}

TEST_P(LockScreenAppManagerImplTest, NoteTakingChangedToLockScreenSupported) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  std::string not_lock_screen_capable_app_id =
      InstallTestApp(kNotLockScreenCapableApp);
  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(),
                                                not_lock_screen_capable_app_id);

  // Initialize app manager - the note taking should be disabled initially
  // because the preferred app is not lock-screen-capable.
  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(false, app_manager()->IsLockScreenAppAvailable());

  // Setting app that is lock-screen-capable as preferred will enable
  // lock screen note taking,
  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  // If test app is installed asynchronously. the app won't be enabled on
  // lock screen until extension service task runner tasks are run.
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  // Verify the lock-screen-capable app is installed in the lock screen app
  // profile.
  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));
  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));

  // Verify the non-lock-screen-capable app was not copied to the lock screen
  // profile (for unpacked apps, the lock screen extension path is the same as
  // the original app path, so it's expected to still exist).
  EXPECT_EQ(
      GetParam() == TestAppType::kUnpackedChromeApp,
      base::PathExists(GetExpectedLockScreenAppPath(kNotLockScreenCapableApp)));

  app_manager()->Stop();

  // Stopping app manager will disable lock screen note taking.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  // Make sure original app paths are not deleted.
  EXPECT_TRUE(PathExists(app_id, profile()));
  EXPECT_TRUE(PathExists(not_lock_screen_capable_app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingReloadedWhileStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));
  EXPECT_EQ(kLockScreenCapableApp.version,
            GetVersion(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, profile()));

  UnloadApp(app_id, profile());

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  // Verify app was unloaded from lock screen profile.
  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  // Add an updated version of the app.
  std::string app_id_updated = InstallTestApp(kLockScreenCapableAppUpdated);
  ASSERT_EQ(app_id, app_id_updated);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));
  EXPECT_EQ(kLockScreenCapableAppUpdated.version,
            GetVersion(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableAppUpdated),
            GetPath(app_id, LockScreenProfile()));

  app_manager()->Stop();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest,
       NoteTakingAppChangeToUnpackedWhileActivating) {
  std::string app_id_1 = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  std::string app_id_2 = InstallTestAppWithType(
      TestAppType::kUnpackedChromeApp, profile(), kLockScreenCapableApp2);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id_2);

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id_2, app_manager()->GetLockScreenAppId());
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id_2, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id_2, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id_2, LockScreenProfile()));

  EXPECT_EQ(
      GetExpectedLockScreenAppPathForAppType(TestAppType::kUnpackedChromeApp,
                                             profile(), kLockScreenCapableApp2),
      GetPath(app_id_2, LockScreenProfile()));

  app_manager()->Stop();

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id_1, profile()));
  EXPECT_TRUE(PathExists(app_id_2, profile()));
}

TEST_P(LockScreenAppManagerImplTest,
       NoteTakingAppChangeToInternalWhileActivating) {
  std::string app_id_1 = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  std::string app_id_2 = InstallTestAppWithType(
      TestAppType::kInternalChromeApp, profile(), kLockScreenCapableApp2);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id_2);

  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id_2, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id_2, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id_2, LockScreenProfile()));

  EXPECT_EQ(
      GetExpectedLockScreenAppPathForAppType(TestAppType::kInternalChromeApp,
                                             profile(), kLockScreenCapableApp2),
      GetPath(app_id_2, LockScreenProfile()));

  app_manager()->Stop();

  RunExtensionServiceTaskRunner(LockScreenProfile());
  RunExtensionServiceTaskRunner(profile());

  EXPECT_TRUE(PathExists(app_id_1, profile()));
  EXPECT_TRUE(PathExists(app_id_2, profile()));
}

TEST_P(LockScreenAppManagerImplTest, ShutdownWhenStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_TRUE(IsInstalled(app_id, LockScreenProfile()));
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWhenEnabled) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  ASSERT_EQ(app_id, app_manager()->GetLockScreenAppId());

  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
  event_observer()->ClearLaunchedApps();

  app_manager()->Stop();

  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_TRUE(event_observer()->launched_apps().empty());
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWithFalseRestoreLastActionState) {
  set_needs_lock_screen_event_router();

  profile()->GetPrefs()->SetBoolean(prefs::kRestoreLastLockScreenNote, false);

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  ASSERT_EQ(app_id, app_manager()->GetLockScreenAppId());

  event_observer()->set_expect_restore_action_state(false);
  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
  event_observer()->ClearLaunchedApps();

  app_manager()->Stop();

  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_TRUE(event_observer()->launched_apps().empty());
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWhenNoLockScreenApp) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/false);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_TRUE(event_observer()->launched_apps().empty());

  app_manager()->Stop();
  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_TRUE(event_observer()->launched_apps().empty());
}

TEST_P(LockScreenAppManagerImplTest, InitializedAfterLockScreenProfileCreated) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  CreateLockScreenProfile();

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/false);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, profile()));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest, StartedBeforeLockScreenProfileCreated) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/false);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  CreateLockScreenProfile();

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_EQ(!IsInstallAsync(), app_manager()->IsLockScreenAppAvailable());

  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(NoteTakingChangedCountOnStart(), note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, LockScreenProfile()));

  EXPECT_EQ(GetExpectedLockScreenAppPath(kLockScreenCapableApp),
            GetPath(app_id, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id, profile()));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest, LockScreenProfileCreatedNoSupportedApp) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/false);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/false);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  CreateLockScreenProfile();
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest, LockScreenProfileCreationFailure) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/false);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  lock_screen_profile_creator()->SetProfileCreationFailed();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
}

TEST_P(LockScreenAppManagerImplTest,
       LockScreenProfileCreationFailedBeforeInitialization) {
  lock_screen_profile_creator()->SetProfileCreationFailed();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/false);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
}

TEST_P(LockScreenAppManagerImplTest, ReloadLockScreenAppAfterAppCrash) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  SimulateAppCrash(app_id, LockScreenProfile());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  // App launch should be successful - this action should reload the
  // terminated app.
  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  // Verify the lock screen note app is enabled.
  ASSERT_TRUE(IsInstalledAndEnabled(app_id, LockScreenProfile()));

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
  event_observer()->ClearLaunchedApps();
}

TEST_P(LockScreenAppManagerImplTest, AppReloadFailure) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  SimulateAppCrash(app_id, LockScreenProfile());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  // Disable the note taking app in the lock screen app profile - this should
  // prevent app reload.
  DisableApp(app_id, LockScreenProfile());

  // App launch should fail - given that the app got disabled, it should not
  // be reloadable anymore.
  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());

  // Make sure that note taking is not reported as available any longer.
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, LockScreenAppGetsUninstalled) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  UninstallApp(app_id, LockScreenProfile());

  // Note taking should be reported to be unavailable if the app was uninstalled
  // from the lock screen profile.
  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, TerminatedAppGetsUninstalled) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  SimulateAppCrash(app_id, LockScreenProfile());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  // Prevent app reload.
  UninstallApp(app_id, LockScreenProfile());

  // Note taking should be reported to be unavailable if the app was uninstalled
  // from the lock screen profile.
  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, DoNotReloadLockScreenAppWhenDisabled) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  DisableApp(app_id, LockScreenProfile());

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest,
       RestartingAppManagerAfterLockScreenAppDisabled) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  DisableApp(app_id, LockScreenProfile());

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  // Restarting the app manager should enable lock screen app again.
  RestartLockScreenAppManager();
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());
  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
}

TEST_P(LockScreenAppManagerImplTest, AppNotReloadedAfterRepeatedCrashes) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner(LockScreenProfile());
  ResetNoteTakingChangedCount();

  // Simulate lock screen note app crash and launch few times.
  for (int i = 0; i < kMaxLockScreenAppReloadsCount; ++i) {
    SimulateAppCrash(app_id, LockScreenProfile());
    EXPECT_TRUE(app_manager()->LaunchLockScreenApp());
  }

  // If app is reloaded too many times, lock screen app should eventually
  // become unavailable.
  SimulateAppCrash(app_id, LockScreenProfile());

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_FALSE(IsInstalledAndEnabled(app_id, LockScreenProfile()));
  event_observer()->ClearLaunchedApps();

  // Restarting the app manager should enable lock screen app again.
  RestartLockScreenAppManager();
  RunExtensionServiceTaskRunner(LockScreenProfile());

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());
  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
}

}  // namespace lock_screen_apps

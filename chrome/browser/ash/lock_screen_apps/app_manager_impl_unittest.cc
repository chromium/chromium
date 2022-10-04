// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/app_manager_impl.h"

#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/traits_bag.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/lock_screen_apps/fake_lock_screen_profile_creator.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/note_taking_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

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

    std::unique_ptr<extensions::api::app_runtime::LaunchData> launch_data =
        extensions::api::app_runtime::LaunchData::FromValue(arg_value);
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
};

enum class TestAppType { kUnpackedChromeApp, kInternalChromeApp, kWebApp };

struct TestApp {
  const char* name = "";
  // GURLs cannot be statically initialized, so use a string.
  const char* url = "";
  const char* extension_id = "";
  const char* version = "";
  bool supports_lock_screen = false;
};

// A lock screen capable app.
const TestApp kLockScreenCapableApp{
    .name = "Lock Screen Capable App",
    .url = "https://lockscreencapable.example.com",
    .extension_id = ash::NoteTakingHelper::kProdKeepExtensionId,
    .version = "1.0",
    .supports_lock_screen = true};
// An updated version of `kLockScreenCapable` (same ID).
const TestApp kLockScreenCapableAppUpdated{
    .name = "Lock Screen Capable App Updated",
    .url = "https://lockscreencapable.example.com",
    .extension_id = ash::NoteTakingHelper::kProdKeepExtensionId,
    .version = "1.1",
    .supports_lock_screen = true};
// Another lock screen capable app (different ID from `kLockScreenCapable`).
const TestApp kLockScreenCapableApp2{
    .name = "Lock Screen Capable App 2",
    .url = "https://lockscreencapable2.example.com",
    .extension_id = ash::NoteTakingHelper::kDevKeepExtensionId,
    .version = "1.0",
    .supports_lock_screen = true};
// A note-taking app that is not lock screen capable.
const TestApp kNotLockScreenCapableApp{
    .name = "Not Lock Screen Capable App",
    .url = "https://notlockscreencapable.example.com",
    .extension_id = ash::NoteTakingHelper::kNoteTakingWebAppIdTest,
    .version = "1.0",
    .supports_lock_screen = false};
const TestApp* const kTestApps[] = {
    &kLockScreenCapableApp, &kLockScreenCapableAppUpdated,
    &kLockScreenCapableApp2, &kNotLockScreenCapableApp};

std::unique_ptr<WebAppInstallInfo> MakeWebAppInfo(const TestApp& test_app) {
  GURL url(test_app.url);
  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(test_app.name);
  info->install_url = url;
  info->start_url = url;
  info->scope = url;
  info->user_display_mode = web_app::UserDisplayMode::kStandalone;
  info->note_taking_new_note_url = url;
  if (test_app.supports_lock_screen)
    info->lock_screen_start_url = url;

  return info;
}

void ModifyExternalInstallOptions(web_app::ExternalInstallOptions& options) {
  for (const TestApp* test_app : kTestApps) {
    if (options.install_url == GURL(test_app->url) &&
        options.fallback_app_name == test_app->name) {
      options.only_use_app_info_factory = true;
      options.app_info_factory = base::BindLambdaForTesting(
          [test_app]() { return MakeWebAppInfo(*test_app); });
      return;
    }
  }
  NOTREACHED() << "Unrecognised app install " << options.install_url.spec()
               << " " << options.fallback_app_name.value_or("(no name)");
}

apps::AppRegistryCache& app_registry_cache(Profile* profile) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  return apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache();
}

bool IsInstalled(const std::string& app_id, Profile* profile) {
  bool result = false;
  app_registry_cache(profile).ForOneApp(
      app_id, [&result](const apps::AppUpdate& update) {
        result = apps_util::IsInstalled(update.Readiness());
      });
  return result;
}

bool IsInstalledWebApp(const std::string& app_id, Profile* profile) {
  bool result = false;
  app_registry_cache(profile).ForOneApp(
      app_id, [&result](const apps::AppUpdate& update) {
        if (apps_util::IsInstalled(update.Readiness()) &&
            update.AppType() == apps::AppType::kWeb) {
          result = true;
        }
      });
  return result;
}

bool IsInstalledAndEnabled(const std::string& app_id, Profile* profile) {
  bool result = false;
  app_registry_cache(profile).ForOneApp(
      app_id, [&result](const apps::AppUpdate& update) {
        result = update.Readiness() == apps::Readiness::kReady;
      });
  return result;
}

class LockScreenAppManagerImplTest
    : public testing::TestWithParam<TestAppType> {
 public:
  LockScreenAppManagerImplTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWebLockScreenApi,
                              blink::features::kWebAppManifestLockScreen},
        /*disabled_features=*/{});
  }

  LockScreenAppManagerImplTest(const LockScreenAppManagerImplTest&) = delete;
  LockScreenAppManagerImplTest& operator=(const LockScreenAppManagerImplTest&) =
      delete;

  ~LockScreenAppManagerImplTest() override = default;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = CreatePrimaryProfile();

    InitExtensionSystem(profile());

    InitWebAppsSystem(profile());

    // Initialize arc session manager - NoteTakingHelper expects it to be set.
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(&ArcSessionFactory)));

    ash::NoteTakingHelper::Initialize();

    lock_screen_profile_creator_ =
        std::make_unique<lock_screen_apps::FakeLockScreenProfileCreator>(
            &profile_manager_);
    lock_screen_profile_creator_->Initialize();
    // Ensure lock screen profiles don't spawn new processes to install apps.
    lock_screen_profile_creator_->AddCreateProfileCallback(base::BindOnce(
        &LockScreenAppManagerImplTest::OnLockScreenProfileCreated,
        base::Unretained(this)));

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

  web_app::FakeWebAppProvider* InitWebAppsSystem(Profile* profile) {
    auto* provider = web_app::FakeWebAppProvider::Get(profile);
    provider->SetDefaultFakeSubsystems();
    provider->SetRunSubsystemStartupTasks(true);
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile);
    return provider;
  }

  void SetUpTestEventRouter() {
    LockScreenEventRouter* event_router =
        extensions::CreateAndUseTestEventRouter<LockScreenEventRouter>(
            LockScreenProfile()->GetOriginalProfile());
    event_observer_ = std::make_unique<LockScreenEventObserver>(
        LockScreenProfile()->GetOriginalProfile());
    event_router->AddEventObserver(event_observer_.get());
  }

  base::FilePath GetChromeAppSourcePath(TestAppType app_type,
                                        Profile* profile,
                                        const std::string& id,
                                        const std::string& version) {
    switch (app_type) {
      case TestAppType::kUnpackedChromeApp:
        return profile->GetPath().Append("Downloads").Append("app");
      case TestAppType::kInternalChromeApp:
        return extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->install_directory()
            .Append(id)
            .Append(version);
      case TestAppType::kWebApp:
        return base::FilePath();
    }
  }

  bool PathExists(const std::string& app_id, Profile* profile) {
    return PathExists(app_id, profile, GetParam());
  }

  bool PathExists(const std::string& app_id,
                  Profile* profile,
                  TestAppType type) {
    // Web Apps don't have a path.
    if (type == TestAppType::kWebApp)
      return IsInstalled(app_id, profile);

    const extensions::Extension* app =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            app_id, extensions::ExtensionRegistry::ENABLED);
    return app && base::PathExists(app->path());
  }

  base::FilePath GetPath(const std::string& app_id, Profile* profile) {
    // Web Apps don't have a path.
    if (GetParam() == TestAppType::kWebApp)
      return base::FilePath();

    const extensions::Extension* app =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            app_id, extensions::ExtensionRegistry::ENABLED);
    return app ? app->path() : base::FilePath();
  }

  base::FilePath GetExpectedLockScreenAppPath(const TestApp& test_app) {
    return GetExpectedLockScreenAppPathForAppType(GetParam(), profile(),
                                                  test_app);
  }

  base::FilePath GetExpectedLockScreenAppPathForAppType(
      TestAppType app_type,
      Profile* original_profile,
      const TestApp& test_app) {
    switch (app_type) {
      case TestAppType::kUnpackedChromeApp:
        return original_profile->GetPath().Append("Downloads").Append("app");
      case TestAppType::kInternalChromeApp:
        return extensions::ExtensionSystem::Get(LockScreenProfile())
            ->extension_service()
            ->install_directory()
            .Append(test_app.extension_id)
            .Append(std::string(test_app.version) + "_0");
      case TestAppType::kWebApp:
        // Web apps don't have a path.
        return base::FilePath();
    }
  }

  extensions::mojom::ManifestLocation GetChromeAppLocation(
      TestAppType app_type) {
    switch (app_type) {
      case TestAppType::kUnpackedChromeApp:
        return extensions::mojom::ManifestLocation::kUnpacked;
      case TestAppType::kInternalChromeApp:
        return extensions::mojom::ManifestLocation::kInternal;
      case TestAppType::kWebApp:
        NOTREACHED();
        return extensions::mojom::ManifestLocation::kInvalidLocation;
    }
  }

  // Returns the ID of installed app.
  std::string InstallTestApp(const TestApp& test_app) {
    return InstallTestAppWithType(test_app, GetParam(), profile());
  }

  // Returns the ID of installed app.
  std::string InstallTestAppWithType(const TestApp& test_app,
                                     TestAppType type,
                                     Profile* profile) {
    if (type == TestAppType::kUnpackedChromeApp ||
        type == TestAppType::kInternalChromeApp) {
      scoped_refptr<const extensions::Extension> extension =
          MakeChromeApp(test_app, type, profile);
      extensions::ExtensionSystem::Get(profile)
          ->extension_service()
          ->AddExtension(extension.get());
      return extension->id();
    } else if (type == TestAppType::kWebApp) {
      return web_app::test::InstallWebApp(
          profile, MakeWebAppInfo(test_app),
          /*overwrite_existing_manifest_fields=*/true,
          webapps::WebappInstallSource::MENU_BROWSER_TAB);
    } else {
      NOTREACHED();
      return std::string();
    }
  }

  scoped_refptr<const extensions::Extension> MakeChromeApp(TestApp test_app,
                                                           TestAppType app_type,
                                                           Profile* profile) {
    if (app_type == TestAppType::kWebApp) {
      NOTREACHED();
      return nullptr;
    }

    std::string id = test_app.extension_id;
    std::string version = test_app.version;
    bool supports_lock_screen = test_app.supports_lock_screen;

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
        GetChromeAppSourcePath(app_type, profile, id, version);

    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(manifest_builder.Build())
            .SetID(id)
            .SetPath(extension_path)
            .SetLocation(GetChromeAppLocation(app_type))
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
    InitWebAppsSystem(profile);
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
    app_manager()->Start(base::BindRepeating(
        &LockScreenAppManagerImplTest::OnNoteTakingAppChanged,
        base::Unretained(this)));
  }

  void RestartLockScreenAppManager() {
    app_manager()->Stop();
    app_manager()->Start(base::BindRepeating(
        &LockScreenAppManagerImplTest::OnNoteTakingAppChanged,
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

  void OnLockScreenProfileCreated() {
    // Skip if profile creation failed.
    if (LockScreenProfile())
      InitWebAppsSystem(LockScreenProfile());
  }

  TestingProfile* profile() { return profile_; }

  Profile* LockScreenProfile() {
    return lock_screen_profile_creator_->lock_screen_profile();
  }

  AppManager* app_manager() { return app_manager_.get(); }

  void ResetAppManager() {
    app_manager_ = std::make_unique<AppManagerImpl>(&tick_clock_);
    // Prevent lock screen web app installs from attempting to fetch URLs.
    app_manager_->OverrideInstallOptions() = &ModifyExternalInstallOptions;
  }

  int note_taking_changed_count() const { return note_taking_changed_count_; }

  void ResetNoteTakingChangedCount() { note_taking_changed_count_ = 0; }

  void AwaitNoteTakingChanged() {
    base::RunLoop run_loop;
    on_note_taking_app_changed_ = run_loop.QuitClosure();
    run_loop.Run();
    on_note_taking_app_changed_.Reset();
  }

  void AwaitNoteTakingChangedCount(int count) {
    base::RunLoop run_loop;
    // Post a task to ensure we don't race with something changing the count.
    if (note_taking_changed_count_ >= count) {
      base::SequencedTaskRunnerHandle::Get()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
      run_loop.Run();
      return;
    }

    on_note_taking_app_changed_ = base::BindLambdaForTesting([&]() {
      if (note_taking_changed_count_ >= count)
        run_loop.Quit();
    });
    run_loop.Run();
    on_note_taking_app_changed_.Reset();
  }

  // Waits for a round trip between file task runner used by the extension
  // service and the main thread - used to ensure that all pending file runner
  // task finish,
  void RunExtensionServiceTaskRunner() {
    base::RunLoop run_loop;
    extensions::GetExtensionFileTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

  bool IsUninstallAsync() { return GetParam() == TestAppType::kWebApp; }

  int NoteTakingChangedCountOnStart() { return 1; }

  LockScreenEventObserver* event_observer() { return event_observer_.get(); }

  FakeLockScreenProfileCreator* lock_screen_profile_creator() {
    return lock_screen_profile_creator_.get();
  }

  // Doubly-parameterised test for changing from the `GetParam()` TestAppType to
  // `new_type` during app activation. Defined below.
  void TestChangeAppTypeWhileActivating(TestAppType new_type);

 protected:
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<lock_screen_apps::FakeLockScreenProfileCreator>
      lock_screen_profile_creator_;

 private:
  void OnNoteTakingAppChanged() {
    ++note_taking_changed_count_;
    if (on_note_taking_app_changed_) {
      on_note_taking_app_changed_.Run();
    }
  }

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
  base::test::ScopedFeatureList feature_list_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::RepeatingClosure on_note_taking_app_changed_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  TestingProfileManager profile_manager_;
  TestingProfile* profile_ = nullptr;

  std::unique_ptr<LockScreenEventObserver> event_observer_;

  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

  std::unique_ptr<AppManagerImpl> app_manager_;

  bool needs_lock_screen_event_router_ = false;
  int note_taking_changed_count_ = 0;
};

absl::optional<std::string> GetVersion(const std::string& app_id,
                                       Profile* profile) {
  std::string app_name;
  app_registry_cache(profile).ForOneApp(
      app_id, [&app_name](const apps::AppUpdate& update) {
        if (apps_util::IsInstalled(update.Readiness()) &&
            update.AppType() == apps::AppType::kWeb) {
          app_name = update.Name();
        }
      });
  for (const TestApp* app : kTestApps) {
    if (app->name == app_name) {
      return app->version;
    }
  }

  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED);
  if (!app)
    return absl::nullopt;
  return app->VersionString();
}

void UnloadApp(const std::string& app_id, Profile* profile) {
  // Web Apps cannot be unloaded, so locally uninstall instead.
  if (IsInstalledWebApp(app_id, profile)) {
    web_app::WebAppProvider::GetForTest(profile)
        ->sync_bridge()
        .SetAppIsLocallyInstalled(app_id, false);
    return;
  }

  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UnloadExtension(app_id, extensions::UnloadedExtensionReason::UPDATE);
}

void SimulateAppCrash(const std::string& app_id, Profile* profile) {
  // Web Apps don't have an equivalent, so just skip them.
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->TerminateExtension(app_id);
}

void DisableApp(const std::string& app_id, Profile* profile) {
  if (IsInstalledWebApp(app_id, profile)) {
    web_app::WebAppProvider::GetForTest(profile)
        ->sync_bridge()
        .SetAppIsDisabled(app_id, true);
    return;
  }

  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->DisableExtension(app_id,
                         extensions::disable_reason::DISABLE_USER_ACTION);
}

void UninstallApp(const std::string& app_id, Profile* profile) {
  if (IsInstalledWebApp(app_id, profile)) {
    base::RunLoop run_loop;
    web_app::WebAppProvider::GetForTest(profile)
        ->install_finalizer()
        .UninstallExternalWebApp(
            app_id, web_app::WebAppManagement::Type::kSystem,
            webapps::WebappUninstallSource::kExternalLockScreen,
            base::BindLambdaForTesting(
                [&run_loop](webapps::UninstallResultCode code) {
                  DCHECK(code == webapps::UninstallResultCode::kSuccess);
                  run_loop.Quit();
                }));
    run_loop.Run();
    return;
  }

  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UninstallExtension(app_id, extensions::UNINSTALL_REASON_FOR_TESTING,
                           nullptr);
}

std::string ToString(TestAppType type) {
  switch (type) {
    case TestAppType::kUnpackedChromeApp:
      return "UnpackedChromeApp";
    case TestAppType::kInternalChromeApp:
      return "InternalChromeApp";
    case TestAppType::kWebApp:
      return "WebApp";
  }
}

std::string ParamToString(testing::TestParamInfo<TestAppType> param) {
  return ToString(param.param);
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         LockScreenAppManagerImplTest,
                         ::testing::Values(TestAppType::kUnpackedChromeApp,
                                           TestAppType::kInternalChromeApp,
                                           TestAppType::kWebApp),
                         ParamToString);

TEST_P(LockScreenAppManagerImplTest, StartAddsAppToTarget) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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
  // Chrome apps are uninstalled at Stop() but web apps are uninstalled only
  // when a different app is installed.
  EXPECT_EQ(IsInstalled(app_id, LockScreenProfile()),
            GetParam() == TestAppType::kWebApp);

  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, StartWhenLockScreenNotesNotEnabled) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/false);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingDisabledWhileStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());

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

  AwaitNoteTakingChangedCount(2);
  ResetNoteTakingChangedCount();

  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  app_manager()->Stop();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingEnabledWhileStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/false);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());

  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  ash::NoteTakingHelper::Get()->SetPreferredAppEnabledOnLockScreen(profile(),
                                                                   true);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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

  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(PathExists(app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingChangedWhileStarted) {
  std::string app_id_1 = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  std::string app_id_2 = InstallTestApp(kLockScreenCapableApp2);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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

  EXPECT_EQ(IsUninstallAsync() ? 1 : 2, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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

  RunExtensionServiceTaskRunner();

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
  RunExtensionServiceTaskRunner();
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(false, app_manager()->IsLockScreenAppAvailable());

  // Setting app that is lock-screen-capable as preferred will enable
  // lock screen note taking,
  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id);

  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  // If test app is installed asynchronously. the app won't be enabled on
  // lock screen until extension service task runner tasks are run.
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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

  RunExtensionServiceTaskRunner();

  // Make sure original app paths are not deleted.
  EXPECT_TRUE(PathExists(app_id, profile()));
  EXPECT_TRUE(PathExists(not_lock_screen_capable_app_id, profile()));
}

TEST_P(LockScreenAppManagerImplTest, LockScreenNoteTakingReloadedWhileStarted) {
  // TODO(crbug.com/1368944): Fix crash on trybots then re-enable this test.
  if (GetParam() == TestAppType::kWebApp)
    return;

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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

  AwaitNoteTakingChangedCount(2);
  RunExtensionServiceTaskRunner();
  EXPECT_EQ(2, note_taking_changed_count());
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
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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

  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(PathExists(app_id, profile()));
}

void LockScreenAppManagerImplTest::TestChangeAppTypeWhileActivating(
    TestAppType new_type) {
  std::string app_id_1 = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  std::string app_id_2 =
      InstallTestAppWithType(kLockScreenCapableApp2, new_type, profile());

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);

  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  ash::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id_2);

  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(note_taking_changed_count(), 1);
  ResetNoteTakingChangedCount();

  RunExtensionServiceTaskRunner();

  AwaitNoteTakingChangedCount(1);
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id_2, app_manager()->GetLockScreenAppId());

  ASSERT_TRUE(IsInstalledAndEnabled(app_id_2, LockScreenProfile()));
  EXPECT_TRUE(PathExists(app_id_2, LockScreenProfile(), new_type));

  app_manager()->Stop();

  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(PathExists(app_id_1, profile()));
  EXPECT_TRUE(PathExists(app_id_2, profile(), new_type));
}

TEST_P(LockScreenAppManagerImplTest,
       NoteTakingAppChangeToUnpackedWhileActivating) {
  TestChangeAppTypeWhileActivating(TestAppType::kUnpackedChromeApp);
}

TEST_P(LockScreenAppManagerImplTest,
       NoteTakingAppChangeToInternalWhileActivating) {
  TestChangeAppTypeWhileActivating(TestAppType::kInternalChromeApp);
}

TEST_P(LockScreenAppManagerImplTest, NoteTakingAppChangeToWebWhileActivating) {
  TestChangeAppTypeWhileActivating(TestAppType::kWebApp);
}

TEST_P(LockScreenAppManagerImplTest, ShutdownWhenStarted) {
  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(IsInstalled(app_id, LockScreenProfile()));
}

TEST_P(LockScreenAppManagerImplTest, LaunchAppWhenEnabled) {
  // TODO(crbug.com/1006642): Enable test when web apps can be launched.
  if (GetParam() == TestAppType::kWebApp)
    return;

  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();

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
  // TODO(crbug.com/1006642): Enable test when web apps can be launched.
  if (GetParam() == TestAppType::kWebApp)
    return;

  set_needs_lock_screen_event_router();

  profile()->GetPrefs()->SetBoolean(prefs::kRestoreLastLockScreenNote, false);

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();

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
  RunExtensionServiceTaskRunner();

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
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();

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
  RunExtensionServiceTaskRunner();

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
  // TODO(crbug.com/1006642): Enable test when web apps can be launched.
  if (GetParam() == TestAppType::kWebApp)
    return;

  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();
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
  // TODO(crbug.com/1006642): Enable test when web apps can be launched.
  if (GetParam() == TestAppType::kWebApp)
    return;

  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();
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
  AwaitNoteTakingChangedCount(1);
  EXPECT_EQ(1, note_taking_changed_count());
  ResetNoteTakingChangedCount();
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
}

TEST_P(LockScreenAppManagerImplTest, LockScreenAppGetsUninstalled) {
  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();
  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  ResetNoteTakingChangedCount();

  UninstallApp(app_id, LockScreenProfile());
  RunExtensionServiceTaskRunner();
  AwaitNoteTakingChangedCount(1);

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
  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();
  ResetNoteTakingChangedCount();

  SimulateAppCrash(app_id, LockScreenProfile());

  // Even though the app was terminated, the observers should not see any state
  // change - the app should be reloaded when launch is requested next time.
  EXPECT_EQ(0, note_taking_changed_count());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());

  // Prevent app reload.
  UninstallApp(app_id, LockScreenProfile());
  AwaitNoteTakingChangedCount(1);

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
  AwaitNoteTakingChangedCount(NoteTakingChangedCountOnStart());
  RunExtensionServiceTaskRunner();
  ResetNoteTakingChangedCount();

  DisableApp(app_id, LockScreenProfile());

  AwaitNoteTakingChangedCount(IsUninstallAsync() ? 2 : 1);
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_TRUE(app_manager()->GetLockScreenAppId().empty());
  EXPECT_FALSE(app_manager()->LaunchLockScreenApp());
  EXPECT_FALSE(IsInstalled(app_id, LockScreenProfile()));

  app_manager()->Stop();
}

TEST_P(LockScreenAppManagerImplTest,
       RestartingAppManagerAfterLockScreenAppDisabled) {
  // TODO(crbug.com/1006642): Enable test when web apps can be launched.
  if (GetParam() == TestAppType::kWebApp)
    return;

  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();
  ResetNoteTakingChangedCount();

  DisableApp(app_id, LockScreenProfile());

  EXPECT_EQ(1, note_taking_changed_count());
  EXPECT_FALSE(app_manager()->IsLockScreenAppAvailable());

  // Restarting the app manager should enable lock screen app again.
  RestartLockScreenAppManager();
  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());
  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
}

TEST_P(LockScreenAppManagerImplTest, AppNotReloadedAfterRepeatedCrashes) {
  // TODO(crbug.com/1006642): Enable test when web apps can be launched.
  if (GetParam() == TestAppType::kWebApp)
    return;

  set_needs_lock_screen_event_router();

  std::string app_id = AddTestAppWithLockScreenSupport(
      kLockScreenCapableApp, /*enable_on_lock_screen=*/true);

  InitializeAndStartAppManager(profile(), /*create_lock_screen_profile=*/true);
  RunExtensionServiceTaskRunner();
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
  RunExtensionServiceTaskRunner();

  EXPECT_TRUE(app_manager()->IsLockScreenAppAvailable());
  EXPECT_EQ(app_id, app_manager()->GetLockScreenAppId());
  EXPECT_TRUE(app_manager()->LaunchLockScreenApp());

  // Verify the lock screen app was sent launch event.
  ASSERT_EQ(1u, event_observer()->launched_apps().size());
  EXPECT_EQ(app_id, event_observer()->launched_apps()[0]);
}

}  // namespace lock_screen_apps

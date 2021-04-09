// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_helper.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/note_taking_controller_client.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_migration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/disks/disk.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/value_builder.h"
#include "url/gurl.h"

namespace app_runtime = extensions::api::app_runtime;

using arc::mojom::IntentHandlerInfo;
using arc::mojom::IntentHandlerInfoPtr;
using base::HistogramTester;
using HandledIntent = arc::FakeIntentHelperInstance::HandledIntent;

namespace chromeos {

using LaunchResult = NoteTakingHelper::LaunchResult;

namespace {

// Name of default profile.
const char kTestProfileName[] = "test-profile";

// Names for keep apps used in tests.
const char kProdKeepAppName[] = "Google Keep [prod]";
const char kDevKeepAppName[] = "Google Keep [dev]";

// Helper functions returning strings that can be used to compare information
// about available note-taking apps.
std::string GetAppString(const std::string& id,
                         const std::string& name,
                         bool preferred,
                         NoteTakingLockScreenSupport lock_screen_support) {
  return base::StringPrintf("{%s, %s, %d, %d}", id.c_str(), name.c_str(),
                            preferred, static_cast<int>(lock_screen_support));
}
std::string GetAppString(const NoteTakingAppInfo& info) {
  return GetAppString(info.app_id, info.name, info.preferred,
                      info.lock_screen_support);
}

// Creates an ARC IntentHandlerInfo object.
IntentHandlerInfoPtr CreateIntentHandlerInfo(const std::string& name,
                                             const std::string& package) {
  IntentHandlerInfoPtr handler = IntentHandlerInfo::New();
  handler->name = name;
  handler->package_name = package;
  return handler;
}

// Converts a filesystem path to an ARC URL.
std::string GetArcUrl(const base::FilePath& path) {
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(
      file_manager::util::ConvertPathToArcUrl(path, &url, &requires_sharing));
  return url.spec();
}

// Implementation of NoteTakingHelper::Observer for testing.
class TestObserver : public NoteTakingHelper::Observer {
 public:
  TestObserver() { NoteTakingHelper::Get()->AddObserver(this); }
  ~TestObserver() override { NoteTakingHelper::Get()->RemoveObserver(this); }

  int num_updates() const { return num_updates_; }
  void reset_num_updates() { num_updates_ = 0; }

  const std::vector<Profile*> preferred_app_updates() const {
    return preferred_app_updates_;
  }
  void clear_preferred_app_updates() { preferred_app_updates_.clear(); }

 private:
  // NoteTakingHelper::Observer:
  void OnAvailableNoteTakingAppsUpdated() override { num_updates_++; }

  void OnPreferredNoteTakingAppUpdated(Profile* profile) override {
    preferred_app_updates_.push_back(profile);
  }

  // Number of times that OnAvailableNoteTakingAppsUpdated() has been called.
  int num_updates_ = 0;

  // Profiles for which OnPreferredNoteTakingAppUpdated was called.
  std::vector<Profile*> preferred_app_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class NoteTakingHelperTest : public BrowserWithTestWindowTest {
 public:
  NoteTakingHelperTest() = default;
  ~NoteTakingHelperTest() override = default;

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::FakeSessionManagerClient::Get()->set_arc_available(true);

    BrowserWithTestWindowTest::SetUp();
    InitExtensionService(profile());
    InitWebAppProvider();
  }

  void TearDown() override {
    if (initialized_) {
      arc::ArcServiceManager::Get()
          ->arc_bridge_service()
          ->intent_helper()
          ->CloseInstance(&intent_helper_);
      arc::ArcServiceManager::Get()
          ->arc_bridge_service()
          ->file_system()
          ->CloseInstance(file_system_.get());
      NoteTakingHelper::Shutdown();
      intent_helper_bridge_.reset();
      file_system_bridge_.reset();
      arc_test_.TearDown();
    }
    extensions::ExtensionSystem::Get(profile())->Shutdown();
    BrowserWithTestWindowTest::TearDown();
    chromeos::SessionManagerClient::Shutdown();
  }

 protected:
  // Information about a Chrome app passed to LaunchChromeApp().
  struct ChromeAppLaunchInfo {
    extensions::ExtensionId id;
    base::FilePath path;
  };

  // Options that can be passed to Init().
  enum InitFlags : uint32_t {
    ENABLE_PLAY_STORE = 1 << 0,
    ENABLE_PALETTE = 1 << 1,
  };

  static NoteTakingHelper* helper() { return NoteTakingHelper::Get(); }

  NoteTakingControllerClient* note_taking_client() {
    return helper()->GetNoteTakingControllerClientForTesting();
  }

  void SetNoteTakingClientProfile(Profile* profile) {
    if (note_taking_client())
      note_taking_client()->SetProfileForTesting(profile);
  }

  // Initializes ARC and NoteTakingHelper. |flags| contains OR-ed together
  // InitFlags values.
  void Init(uint32_t flags) {
    ASSERT_FALSE(initialized_);
    initialized_ = true;

    profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled,
                                      flags & ENABLE_PLAY_STORE);
    arc_test_.SetUp(profile());
    // Set up ArcIntentHelperBridge to emulate full-duplex IntentHelper
    // connection.
    intent_helper_bridge_ = std::make_unique<arc::ArcIntentHelperBridge>(
        profile(), arc::ArcServiceManager::Get()->arc_bridge_service());
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(&intent_helper_);
    WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper());

    file_system_bridge_ = std::make_unique<arc::ArcFileSystemBridge>(
        profile(), arc::ArcServiceManager::Get()->arc_bridge_service());
    file_system_ = std::make_unique<arc::FakeFileSystemInstance>();

    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->file_system()
        ->SetInstance(file_system_.get());
    WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->file_system());
    ASSERT_TRUE(file_system_->InitCalled());

    if (flags & ENABLE_PALETTE) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kAshForceEnableStylusTools);
    }

    // TODO(derat): Sigh, something in ArcAppTest appears to be re-enabling ARC.
    profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled,
                                      flags & ENABLE_PLAY_STORE);
    NoteTakingHelper::Initialize();
    NoteTakingHelper::Get()->SetProfileWithEnabledLockScreenApps(profile());
    NoteTakingHelper::Get()->set_launch_chrome_app_callback_for_test(
        base::BindRepeating(&NoteTakingHelperTest::LaunchChromeApp,
                            base::Unretained(this)));
  }

  // Creates an extension.
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name) {
    return CreateExtension(id, name, nullptr, nullptr);
  }
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name,
      std::unique_ptr<base::Value> permissions,
      std::unique_ptr<base::Value> action_handlers) {
    std::unique_ptr<base::DictionaryValue> manifest =
        extensions::DictionaryBuilder()
            .Set("name", name)
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("app",
                 extensions::DictionaryBuilder()
                     .Set("background",
                          extensions::DictionaryBuilder()
                              .Set("scripts", extensions::ListBuilder()
                                                  .Append("background.js")
                                                  .Build())
                              .Build())
                     .Build())
            .Build();

    if (action_handlers)
      manifest->Set("action_handlers", std::move(action_handlers));

    if (permissions)
      manifest->Set("permissions", std::move(permissions));

    return extensions::ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(id)
        .Build();
  }

  void InitWebAppProvider() {
    auto* provider = web_app::TestWebAppProvider::Get(profile());
    // Migration manager won't complete initialization due to using a test
    // extensions system that is never started. Not needed so just disable it.
    provider->SetMigrationManager(nullptr);
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  // Initializes extensions-related objects for |profile|. Tests only need to
  // call this if they create additional profiles of their own.
  void InitExtensionService(Profile* profile) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
  }

  // Installs or uninstalls |extension| in |profile|.
  void InstallExtension(const extensions::Extension* extension,
                        Profile* profile) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->AddExtension(extension);
  }
  void UninstallExtension(const extensions::Extension* extension,
                          Profile* profile) {
    std::u16string error;
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->UninstallExtension(
            extension->id(),
            extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING, &error);
  }

  scoped_refptr<const extensions::Extension> CreateAndInstallLockScreenApp(
      const std::string& id,
      const std::string& app_name,
      Profile* profile) {
    return CreateAndInstallLockScreenAppWithPermissions(
        id, app_name, extensions::ListBuilder().Append("lockScreen").Build(),
        profile);
  }

  scoped_refptr<const extensions::Extension>
  CreateAndInstallLockScreenAppWithPermissions(
      const std::string& id,
      const std::string& app_name,
      std::unique_ptr<base::ListValue> permissions,
      Profile* profile) {
    std::unique_ptr<base::Value> lock_enabled_action_handler =
        extensions::ListBuilder()
            .Append(extensions::DictionaryBuilder()
                        .Set("action", app_runtime::ToString(
                                           app_runtime::ACTION_TYPE_NEW_NOTE))
                        .Set("enabled_on_lock_screen", true)
                        .Build())
            .Build();

    scoped_refptr<const extensions::Extension> keep_extension =
        CreateExtension(id, app_name, std::move(permissions),
                        std::move(lock_enabled_action_handler));
    InstallExtension(keep_extension.get(), profile);

    return keep_extension;
  }

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    profile_prefs_ = prefs.get();
    return profile_manager()->CreateTestingProfile(
        kTestProfileName, std::move(prefs), u"Test profile", 1 /*avatar_id*/,
        std::string() /*supervised_user_id*/,
        TestingProfile::TestingFactories());
  }

  testing::AssertionResult PreferredAppMatches(Profile* profile,
                                               NoteTakingAppInfo app_info) {
    std::unique_ptr<NoteTakingAppInfo> preferred_app =
        helper()->GetPreferredLockScreenAppInfo(profile);
    if (!preferred_app)
      return ::testing::AssertionFailure() << "No preferred app";

    std::string expected = GetAppString(app_info);
    std::string actual = GetAppString(*preferred_app);
    if (expected != actual) {
      return ::testing::AssertionFailure() << "Expected: " << expected << " "
                                           << "Actual: " << actual;
    }
    return ::testing::AssertionSuccess();
  }

  std::string NoteAppInfoListToString(
      const std::vector<NoteTakingAppInfo>& apps) {
    std::vector<std::string> app_strings;
    for (const auto& app : apps)
      app_strings.push_back(GetAppString(app));
    return base::JoinString(app_strings, ",");
  }

  testing::AssertionResult AvailableAppsMatch(
      Profile* profile,
      const std::vector<NoteTakingAppInfo>& expected_apps) {
    std::vector<NoteTakingAppInfo> actual_apps =
        helper()->GetAvailableApps(profile);
    if (actual_apps.size() != expected_apps.size()) {
      return ::testing::AssertionFailure()
             << "Size mismatch. "
             << "Expected: [" << NoteAppInfoListToString(expected_apps) << "] "
             << "Actual: [" << NoteAppInfoListToString(actual_apps) << "]";
    }

    std::unique_ptr<::testing::AssertionResult> failure;
    for (size_t i = 0; i < expected_apps.size(); ++i) {
      std::string expected = GetAppString(expected_apps[i]);
      std::string actual = GetAppString(actual_apps[i]);
      if (expected != actual) {
        if (!failure) {
          failure = std::make_unique<::testing::AssertionResult>(
              ::testing::AssertionFailure());
        }
        *failure << "Error at index " << i << ": "
                 << "Expected: " << expected << " "
                 << "Actual: " << actual;
      }
    }

    if (failure)
      return *failure;
    return ::testing::AssertionSuccess();
  }

  // Info about launched Chrome apps, in the order they were launched.
  std::vector<ChromeAppLaunchInfo> launched_chrome_apps_;

  arc::FakeIntentHelperInstance intent_helper_;

  std::unique_ptr<arc::ArcFileSystemBridge> file_system_bridge_;

  std::unique_ptr<arc::FakeFileSystemInstance> file_system_;

  // Pointer to the primary profile (returned by |profile()|) prefs - owned by
  // the profile.
  sync_preferences::TestingPrefServiceSyncable* profile_prefs_ = nullptr;

 private:
  // Callback registered with the helper to record Chrome app launch requests.
  void LaunchChromeApp(content::BrowserContext* passed_context,
                       const extensions::Extension* extension,
                       std::unique_ptr<app_runtime::ActionData> action_data,
                       const base::FilePath& path) {
    EXPECT_EQ(profile(), passed_context);
    EXPECT_EQ(app_runtime::ActionType::ACTION_TYPE_NEW_NOTE,
              action_data->action_type);
    launched_chrome_apps_.push_back(ChromeAppLaunchInfo{extension->id(), path});
  }

  // Has Init() been called?
  bool initialized_ = false;

  ArcAppTest arc_test_;
  std::unique_ptr<arc::ArcIntentHelperBridge> intent_helper_bridge_;

  DISALLOW_COPY_AND_ASSIGN(NoteTakingHelperTest);
};

TEST_F(NoteTakingHelperTest, PaletteNotEnabled) {
  // Without the palette enabled, IsAppAvailable() should return false.
  Init(0);
  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get(), profile());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
}

TEST_F(NoteTakingHelperTest, ListChromeApps) {
  Init(ENABLE_PALETTE);

  // Start out without any note-taking apps installed.
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // If only the prod version of the app is installed, it should be returned.
  scoped_refptr<const extensions::Extension> prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, kProdKeepAppName);
  InstallExtension(prod_extension.get(), profile());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));

  // If the dev version is also installed, it should be listed before the prod
  // version.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, kDevKeepAppName);
  InstallExtension(dev_extension.get(), profile());
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredLockScreenAppInfo(profile()));

  // Now install a random web app to check that it's ignored.
  web_app::test::InstallDummyWebApp(profile(), "Web App",
                                    GURL("http://some.url"));
  // Now install a random extension to check that it's ignored.
  const extensions::ExtensionId kOtherId = crx_file::id_util::GenerateId("a");
  const std::string kOtherName = "Some Other App";
  scoped_refptr<const extensions::Extension> other_extension =
      CreateExtension(kOtherId, kOtherName);
  InstallExtension(other_extension.get(), profile());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredLockScreenAppInfo(profile()));

  // Mark the prod version as preferred.
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kProdKeepExtensionId);
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));

  EXPECT_TRUE(PreferredAppMatches(
      profile(),
      {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
       true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}));
}

TEST_F(NoteTakingHelperTest, ListChromeAppsWithLockScreenNotesSupported) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  std::unique_ptr<base::Value> lock_disabled_action_handler =
      extensions::ListBuilder()
          .Append(app_runtime::ToString(app_runtime::ACTION_TYPE_NEW_NOTE))
          .Build();

  // Install Keep app that does not support lock screen note taking - it should
  // be reported not to support lock screen note taking.
  scoped_refptr<const extensions::Extension> prod_extension = CreateExtension(
      NoteTakingHelper::kProdKeepExtensionId, kProdKeepAppName,
      nullptr /* permissions */, std::move(lock_disabled_action_handler));
  InstallExtension(prod_extension.get(), profile());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredLockScreenAppInfo(profile()));

  // Install additional Keep app - one that supports lock screen note taking.
  // This app should be reported to support note taking (given that
  // enable-lock-screen-apps flag is set).
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredLockScreenAppInfo(profile()));
}

TEST_F(NoteTakingHelperTest, PreferredAppEnabledOnLockScreen) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // Install lock screen enabled Keep note taking app.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());

  // Verify that the app is reported to support lock screen note taking.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kEnabled}}));
  EXPECT_FALSE(helper()->GetPreferredLockScreenAppInfo(profile()));

  // When the lock screen note taking pref is set and the Keep app is set as the
  // preferred note taking app, the app should be reported as selected as lock
  // screen note taking app.
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kDevKeepExtensionId);
  helper()->SetPreferredAppEnabledOnLockScreen(profile(), true);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kEnabled}}));
  EXPECT_TRUE(PreferredAppMatches(
      profile(), {kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
                  true /*preferred*/, NoteTakingLockScreenSupport::kEnabled}));

  helper()->SetPreferredAppEnabledOnLockScreen(profile(), false);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kSupported}}));
  EXPECT_TRUE(PreferredAppMatches(
      profile(),
      {kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
       true /*preferred*/, NoteTakingLockScreenSupport::kSupported}));
}

TEST_F(NoteTakingHelperTest, PreferredAppWithNoLockScreenPermission) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // Install lock screen enabled Keep note taking app, but wihtout lock screen
  // permission listed.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenAppWithPermissions(
          NoteTakingHelper::kDevKeepExtensionId, kDevKeepAppName, nullptr,
          profile());
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kDevKeepExtensionId);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(PreferredAppMatches(
      profile(),
      {kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
       true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}));
}

TEST_F(NoteTakingHelperTest,
       PreferredAppWithotLockSupportClearsLockScreenPref) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // Install dev Keep app that supports lock screen note taking.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());

  // Install third-party app that doesn't support lock screen note taking.
  const extensions::ExtensionId kNewNoteId = crx_file::id_util::GenerateId("a");
  const std::string kName = "Some App";
  scoped_refptr<const extensions::Extension> has_new_note =
      CreateAndInstallLockScreenAppWithPermissions(kNewNoteId, kName, nullptr,
                                                   profile());

  // Verify that only Keep app is reported to support lock screen note taking.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
                   false /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
                  {kName, kNewNoteId, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));

  // When the Keep app is set as preferred app, and note taking on lock screen
  // is enabled, the keep app should be reported to be selected as the lock
  // screen note taking app.
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kDevKeepExtensionId);
  helper()->SetPreferredAppEnabledOnLockScreen(profile(), true);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
                   true /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
                  {kName, kNewNoteId, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));

  // When a third party app (which does not support lock screen note taking) is
  // set as the preferred app, Keep app's lock screen support state remain
  // enabled - even though it will not be launchable from the lock screen.
  helper()->SetPreferredApp(profile(), kNewNoteId);
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
                   false /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
                  {kName, kNewNoteId, true /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(PreferredAppMatches(
      profile(), {kName, kNewNoteId, true /*preferred*/,
                  NoteTakingLockScreenSupport::kNotSupported}));
}

// Verify the note helper detects apps with "new_note" "action_handler" manifest
// entries.
TEST_F(NoteTakingHelperTest, CustomChromeApps) {
  Init(ENABLE_PALETTE);

  const extensions::ExtensionId kNewNoteId = crx_file::id_util::GenerateId("a");
  const extensions::ExtensionId kEmptyArrayId =
      crx_file::id_util::GenerateId("b");
  const extensions::ExtensionId kEmptyId = crx_file::id_util::GenerateId("c");
  const std::string kName = "Some App";

  // "action_handlers": ["new_note"]
  scoped_refptr<const extensions::Extension> has_new_note = CreateExtension(
      kNewNoteId, kName, nullptr /* permissions */,
      extensions::ListBuilder()
          .Append(app_runtime::ToString(app_runtime::ACTION_TYPE_NEW_NOTE))
          .Build());
  InstallExtension(has_new_note.get(), profile());
  // "action_handlers": []
  scoped_refptr<const extensions::Extension> empty_array =
      CreateExtension(kEmptyArrayId, kName, nullptr /* permissions*/,
                      extensions::ListBuilder().Build());
  InstallExtension(empty_array.get(), profile());
  // (no action handler entry)
  scoped_refptr<const extensions::Extension> none =
      CreateExtension(kEmptyId, kName);
  InstallExtension(none.get(), profile());

  // Only the "new_note" extension is returned from GetAvailableApps.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName, kNewNoteId, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));
}

// Web apps with or without a note_taking_new_note_url are not yet supported.
TEST_F(NoteTakingHelperTest, CustomWebApps) {
  Init(ENABLE_PALETTE);

  {
    auto app_info = std::make_unique<WebApplicationInfo>();
    app_info->start_url = GURL("http://some1.url");
    app_info->scope = GURL("http://some1.url");
    app_info->title = u"Web App 1";
    web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  {
    auto app_info = std::make_unique<WebApplicationInfo>();
    app_info->start_url = GURL("http://some2.url");
    app_info->scope = GURL("http://some2.url");
    app_info->title = u"Web App 2";
    // Set a note_taking_new_note_url on one app.
    app_info->note_taking_new_note_url = GURL("http://some2.url/new-note");
    web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  // Check apps were installed.
  auto* provider = web_app::WebAppProvider::Get(profile());
  EXPECT_EQ(provider->registrar().CountUserInstalledApps(), 2);

  // Apps with note_taking_new_note_url are not yet supported.
  EXPECT_TRUE(AvailableAppsMatch(profile(), {}));
}

// Verify that non-allowlisted apps cannot be enabled on lock screen.
TEST_F(NoteTakingHelperTest, CustomLockScreenEnabledApps) {
  Init(ENABLE_PALETTE);

  const extensions::ExtensionId kNewNoteId = crx_file::id_util::GenerateId("a");
  const std::string kName = "Some App";

  scoped_refptr<const extensions::Extension> extension =
      CreateAndInstallLockScreenApp(kNewNoteId, kName, profile());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName, kNewNoteId, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));
}

TEST_F(NoteTakingHelperTest, AllowlistedAndCustomAppsShowOnlyOnce) {
  Init(ENABLE_PALETTE);

  scoped_refptr<const extensions::Extension> extension = CreateExtension(
      NoteTakingHelper::kProdKeepExtensionId, "Keep", nullptr /* permissions */,
      extensions::ListBuilder()
          .Append(app_runtime::ToString(app_runtime::ACTION_TYPE_NEW_NOTE))
          .Build());
  InstallExtension(extension.get(), profile());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{"Keep", NoteTakingHelper::kProdKeepExtensionId, false /*preferred*/,
        NoteTakingLockScreenSupport::kNotSupported}}));
}

TEST_F(NoteTakingHelperTest, LaunchChromeApp) {
  Init(ENABLE_PALETTE);
  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get(), profile());

  // Check the Chrome app is launched with the correct parameters.
  HistogramTester histogram_tester;
  const base::FilePath kPath("/foo/bar/photo.jpg");
  helper()->LaunchAppForNewNote(profile(), kPath);
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);
  EXPECT_EQ(kPath, launched_chrome_apps_[0].path);

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
}

TEST_F(NoteTakingHelperTest, LaunchWebApp) {
  Init(ENABLE_PALETTE);
  // Install a default-allowed web app corresponding to ID of
  // |NoteTakingHelper::kNoteTakingWebAppIdTest|.
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->start_url = GURL("https://yielding-large-chef.glitch.me/");
  app_info->title = u"Default Allowed Web App";
  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(app_info));
  EXPECT_EQ(app_id, NoteTakingHelper::kNoteTakingWebAppIdTest);

  // Check the web app is launched with the correct parameters.
  HistogramTester histogram_tester;
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  // Web app, so no launched_chrome_apps.
  EXPECT_EQ(0u, launched_chrome_apps_.size());

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::WEB_APP_SUCCESS), 1);
}

TEST_F(NoteTakingHelperTest, FallBackIfPreferredAppUnavailable) {
  Init(ENABLE_PALETTE);
  scoped_refptr<const extensions::Extension> prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "prod");
  InstallExtension(prod_extension.get(), profile());
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, "dev");
  InstallExtension(dev_extension.get(), profile());
  {
    // Install a default-allowed web app corresponding to ID of
    // |NoteTakingHelper::kNoteTakingWebAppIdTest|.
    auto app_info = std::make_unique<WebApplicationInfo>();
    app_info->start_url = GURL("https://yielding-large-chef.glitch.me/");
    app_info->title = u"Default Allowed Web App";
    std::string app_id =
        web_app::test::InstallWebApp(profile(), std::move(app_info));
    EXPECT_EQ(app_id, NoteTakingHelper::kNoteTakingWebAppIdTest);
  }

  // Set the prod app as preferred and check that it's launched.
  std::unique_ptr<HistogramTester> histogram_tester(new HistogramTester());
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kProdKeepExtensionId);
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  ASSERT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
  histogram_tester->ExpectTotalCount(
      NoteTakingHelper::kDefaultLaunchResultHistogramName, 0);

  // Now uninstall the prod app and check that we fall back to the dev app.
  UninstallExtension(prod_extension.get(), profile());
  launched_chrome_apps_.clear();
  histogram_tester.reset(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(NoteTakingHelper::kDevKeepExtensionId, launched_chrome_apps_[0].id);

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_APP_MISSING), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);

  // Now uninstall the dev app and check that we fall back to the test web app.
  UninstallExtension(dev_extension.get(), profile());
  launched_chrome_apps_.clear();
  histogram_tester.reset(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  // Not a chrome app.
  EXPECT_EQ(0u, launched_chrome_apps_.size());

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_APP_MISSING), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::WEB_APP_SUCCESS), 1);
}

TEST_F(NoteTakingHelperTest, PlayStoreInitiallyDisabled) {
  Init(ENABLE_PALETTE);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  // When Play Store is enabled, the helper's members should be updated
  // accordingly.
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  // After the callback to receive intent handlers has run, the "apps received"
  // member should be updated (even if there aren't any apps).
  helper()->OnIntentFiltersUpdated(base::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
}

TEST_F(NoteTakingHelperTest, AddProfileWithPlayStoreEnabled) {
  Init(ENABLE_PALETTE);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  TestObserver observer;
  ASSERT_EQ(0, observer.num_updates());

  // Add a second profile with the ARC-enabled pref already set. The Play Store
  // should be immediately regarded as being enabled and the observer should be
  // notified, since OnArcPlayStoreEnabledChanged() apparently isn't called in
  // this case: http://crbug.com/700554
  const char kSecondProfileName[] = "second-profile";
  auto prefs = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());
  prefs->SetBoolean(arc::prefs::kArcEnabled, true);
  profile_manager()->CreateTestingProfile(
      kSecondProfileName, std::move(prefs), u"Second User", 1 /* avatar_id */,
      std::string() /* supervised_user_id */,
      TestingProfile::TestingFactories());
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_EQ(1, observer.num_updates());

  // TODO(derat|hidehiko): Check that NoteTakingHelper adds itself as an
  // observer of the ArcIntentHelperBridge corresponding to the new profile:
  // https://crbug.com/748763

  // Notification of updated intent filters should result in the apps being
  // refreshed.
  helper()->OnIntentFiltersUpdated(base::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
  EXPECT_EQ(2, observer.num_updates());

  profile_manager()->DeleteTestingProfile(kSecondProfileName);
}

TEST_F(NoteTakingHelperTest, ListAndroidApps) {
  // Add two Android apps.
  std::vector<IntentHandlerInfoPtr> handlers;
  const std::string kName1 = "App 1";
  const std::string kPackage1 = "org.chromium.package1";
  handlers.emplace_back(CreateIntentHandlerInfo(kName1, kPackage1));
  const std::string kName2 = "App 2";
  const std::string kPackage2 = "org.chromium.package2";
  handlers.emplace_back(CreateIntentHandlerInfo(kName2, kPackage2));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  // NoteTakingHelper should make an async request for Android apps when
  // constructed.
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // The apps should be listed after the callback has had a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName1, kPackage1, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported},
                  {kName2, kPackage2, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));

  helper()->SetPreferredApp(profile(), kPackage1);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName1, kPackage1, true /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported},
                  {kName2, kPackage2, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_FALSE(helper()->GetPreferredLockScreenAppInfo(profile()));

  // Disable Play Store and check that the apps are no longer returned.
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());
}

TEST_F(NoteTakingHelperTest, LaunchAndroidApp) {
  const std::string kPackage1 = "org.chromium.package1";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App 1", kPackage1));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper()->IsAppAvailable(profile()));

  // The installed app should be launched.
  std::unique_ptr<HistogramTester> histogram_tester(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, file_system_->handledUrlRequests().size());
  EXPECT_EQ(arc::mojom::ActionType::CREATE_NOTE,
            file_system_->handledUrlRequests().at(0)->action_type);
  EXPECT_EQ(
      kPackage1,
      file_system_->handledUrlRequests().at(0)->activity_name->package_name);
  EXPECT_EQ(
      std::string(),
      file_system_->handledUrlRequests().at(0)->activity_name->activity_name);
  ASSERT_EQ(0u, file_system_->handledUrlRequests().at(0)->urls.size());

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::ANDROID_SUCCESS), 1);

  // Install a second app and set it as the preferred app.
  const std::string kPackage2 = "org.chromium.package2";
  handlers.emplace_back(CreateIntentHandlerInfo("App 1", kPackage1));
  handlers.emplace_back(CreateIntentHandlerInfo("App 2", kPackage2));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));
  helper()->OnIntentFiltersUpdated(base::nullopt);
  base::RunLoop().RunUntilIdle();
  helper()->SetPreferredApp(profile(), kPackage2);

  // The second app should be launched now.
  intent_helper_.clear_handled_intents();
  file_system_->clear_handled_requests();
  histogram_tester.reset(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, file_system_->handledUrlRequests().size());
  EXPECT_EQ(arc::mojom::ActionType::CREATE_NOTE,
            file_system_->handledUrlRequests().at(0)->action_type);
  EXPECT_EQ(
      kPackage2,
      file_system_->handledUrlRequests().at(0)->activity_name->package_name);
  EXPECT_EQ(
      std::string(),
      file_system_->handledUrlRequests().at(0)->activity_name->activity_name);
  ASSERT_EQ(0u, file_system_->handledUrlRequests().at(0)->urls.size());

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::ANDROID_SUCCESS), 1);
  histogram_tester->ExpectTotalCount(
      NoteTakingHelper::kDefaultLaunchResultHistogramName, 0);
}

TEST_F(NoteTakingHelperTest, LaunchAndroidAppWithPath) {
  chromeos::disks::DiskMountManager::InitializeForTesting(
      new file_manager::FakeDiskMountManager);

  ASSERT_TRUE(chromeos::disks::DiskMountManager::GetInstance()->AddDiskForTest(
      chromeos::disks::Disk::Builder()
          .SetDevicePath("/device/source_path")
          .SetFileSystemUUID("0123-abcd")
          .Build()));
  ASSERT_TRUE(
      chromeos::disks::DiskMountManager::GetInstance()->AddMountPointForTest(
          chromeos::disks::DiskMountManager::MountPointInfo(
              "/device/source_path", "/media/removable/UNTITLED",
              chromeos::MOUNT_TYPE_DEVICE,
              chromeos::disks::MOUNT_CONDITION_NONE)));

  const std::string kPackage = "org.chromium.package";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App", kPackage));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper()->IsAppAvailable(profile()));

  const base::FilePath kDownloadedPath(
      file_manager::util::GetDownloadsFolderForProfile(profile()).Append(
          "image.jpg"));
  helper()->LaunchAppForNewNote(profile(), kDownloadedPath);
  ASSERT_EQ(1u, file_system_->handledUrlRequests().size());
  EXPECT_EQ(arc::mojom::ActionType::CREATE_NOTE,
            file_system_->handledUrlRequests().at(0)->action_type);
  EXPECT_EQ(
      kPackage,
      file_system_->handledUrlRequests().at(0)->activity_name->package_name);
  EXPECT_EQ(
      std::string(),
      file_system_->handledUrlRequests().at(0)->activity_name->activity_name);
  ASSERT_EQ(1u, file_system_->handledUrlRequests().at(0)->urls.size());
  ASSERT_EQ(GetArcUrl(kDownloadedPath),
            file_system_->handledUrlRequests().at(0)->urls.at(0)->content_url);

  const base::FilePath kRemovablePath =
      base::FilePath(file_manager::util::kRemovableMediaPath)
          .Append("UNTITLED/image.jpg");
  intent_helper_.clear_handled_intents();
  file_system_->clear_handled_requests();
  helper()->LaunchAppForNewNote(profile(), kRemovablePath);
  ASSERT_EQ(1u, file_system_->handledUrlRequests().size());
  EXPECT_EQ(arc::mojom::ActionType::CREATE_NOTE,
            file_system_->handledUrlRequests().at(0)->action_type);
  EXPECT_EQ(
      kPackage,
      file_system_->handledUrlRequests().at(0)->activity_name->package_name);
  EXPECT_EQ(
      std::string(),
      file_system_->handledUrlRequests().at(0)->activity_name->activity_name);
  ASSERT_EQ(1u, file_system_->handledUrlRequests().at(0)->urls.size());
  ASSERT_EQ(GetArcUrl(kRemovablePath),
            file_system_->handledUrlRequests().at(0)->urls.at(0)->content_url);

  // When a path that isn't accessible to ARC is passed, the request should be
  // dropped.
  HistogramTester histogram_tester;
  intent_helper_.clear_handled_intents();
  file_system_->clear_handled_requests();
  helper()->LaunchAppForNewNote(profile(), base::FilePath("/bad/path.jpg"));
  EXPECT_TRUE(file_system_->handledUrlRequests().empty());

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::ANDROID_FAILED_TO_CONVERT_PATH), 1);

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(NoteTakingHelperTest, NoAppsAvailable) {
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);

  // When no note-taking apps are installed, the histograms should just be
  // updated.
  HistogramTester histogram_tester;
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APPS_AVAILABLE), 1);
}

TEST_F(NoteTakingHelperTest, NotifyObserverAboutAndroidApps) {
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  TestObserver observer;

  // Let the app-fetching callback run and check that the observer is notified.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.num_updates());

  // Disabling and enabling Play Store should also notify the observer (and
  // enabling should request apps again).
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);
  EXPECT_EQ(2, observer.num_updates());
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  EXPECT_EQ(3, observer.num_updates());
  // Run ARC data removing operation.
  base::RunLoop().RunUntilIdle();

  // Update intent filters and check that the observer is notified again after
  // apps are received.
  helper()->OnIntentFiltersUpdated(base::nullopt);
  EXPECT_EQ(3, observer.num_updates());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, observer.num_updates());
}

TEST_F(NoteTakingHelperTest, NotifyObserverAboutChromeApps) {
  Init(ENABLE_PALETTE);
  TestObserver observer;
  ASSERT_EQ(0, observer.num_updates());

  // Notify that the prod Keep app was installed for the initial profile. Chrome
  // extensions are queried dynamically when GetAvailableApps() is called, so we
  // don't need to actually install it.
  scoped_refptr<const extensions::Extension> keep_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(keep_extension.get(), profile());
  EXPECT_EQ(1, observer.num_updates());

  // Unloading the extension should also trigger a notification.
  UninstallExtension(keep_extension.get(), profile());
  EXPECT_EQ(2, observer.num_updates());

  // Non-note-taking apps shouldn't trigger notifications.
  scoped_refptr<const extensions::Extension> other_extension =
      CreateExtension(crx_file::id_util::GenerateId("a"), "Some Other App");
  InstallExtension(other_extension.get(), profile());
  EXPECT_EQ(2, observer.num_updates());
  UninstallExtension(other_extension.get(), profile());
  EXPECT_EQ(2, observer.num_updates());

  // Add a second profile and check that it triggers notifications too.
  observer.reset_num_updates();
  const std::string kSecondProfileName = "second-profile";
  TestingProfile* second_profile =
      profile_manager()->CreateTestingProfile(kSecondProfileName);
  InitExtensionService(second_profile);
  EXPECT_EQ(0, observer.num_updates());
  InstallExtension(keep_extension.get(), second_profile);
  EXPECT_EQ(1, observer.num_updates());
  UninstallExtension(keep_extension.get(), second_profile);
  EXPECT_EQ(2, observer.num_updates());
  profile_manager()->DeleteTestingProfile(kSecondProfileName);
}

TEST_F(NoteTakingHelperTest, NotifyObserverAboutPreferredAppChanges) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  scoped_refptr<const extensions::Extension> prod_keep_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(prod_keep_extension.get(), profile());

  scoped_refptr<const extensions::Extension> dev_keep_extension =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, "Keep");
  InstallExtension(dev_keep_extension.get(), profile());

  ASSERT_TRUE(observer.preferred_app_updates().empty());

  // Observers should be notified when preferred app is set.
  helper()->SetPreferredApp(profile(), prod_keep_extension->id());
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // If the preferred app is not changed, observers should not be notified.
  helper()->SetPreferredApp(profile(), prod_keep_extension->id());
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Observers should be notified when preferred app is changed.
  helper()->SetPreferredApp(profile(), dev_keep_extension->id());
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Observers should be notified when preferred app is cleared.
  helper()->SetPreferredApp(profile(), "");
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // No change to preferred app.
  helper()->SetPreferredApp(profile(), "");
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Initialize secondary profile with a test app.
  const std::string kSecondProfileName = "second-profile";
  TestingProfile* second_profile =
      profile_manager()->CreateTestingProfile(kSecondProfileName);
  InitExtensionService(second_profile);
  scoped_refptr<const extensions::Extension>
      second_profile_prod_keep_extension =
          CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(second_profile_prod_keep_extension.get(), second_profile);

  // Verify that observers are called with the scondary profile if the secondary
  // profile preferred app changes.
  helper()->SetPreferredApp(second_profile,
                            second_profile_prod_keep_extension->id());
  EXPECT_EQ(std::vector<Profile*>{second_profile},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Clearing preferred app in secondary ptofile should fire observers with the
  // secondary profile.
  helper()->SetPreferredApp(second_profile, "");
  EXPECT_EQ(std::vector<Profile*>{second_profile},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  profile_manager()->DeleteTestingProfile(kSecondProfileName);
}

TEST_F(NoteTakingHelperTest,
       NotifyObserverAboutPreferredAppLockScreenSupportChanges) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());

  scoped_refptr<const extensions::Extension> prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(prod_extension.get(), profile());

  ASSERT_TRUE(observer.preferred_app_updates().empty());

  // Set the app that supports lock screen note taking as preferred.
  helper()->SetPreferredApp(profile(), dev_extension->id());
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Disable the preferred app on the lock screen.
  EXPECT_TRUE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Disabling lock screen support for already enabled app should be no-op.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Change the state of the preferred app - it should succeed, and a
  // notification should be fired.
  EXPECT_TRUE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // No-op, because the preferred app state is not changing.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Set an app that does not support lock screen as primary.
  helper()->SetPreferredApp(profile(), prod_extension->id());
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Chaning state for an app that does not support lock screen note taking
  // should be no-op.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_TRUE(observer.preferred_app_updates().empty());
}

TEST_F(NoteTakingHelperTest, SetAppEnabledOnLockScreen) {
  Init(ENABLE_PALETTE);

  TestObserver observer;

  scoped_refptr<const extensions::Extension> dev_app =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());
  scoped_refptr<const extensions::Extension> prod_app =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kProdKeepExtensionId,
                                    kProdKeepAppName, profile());
  const std::string kUnsupportedAppName = "App name";
  const extensions::ExtensionId kUnsupportedAppId =
      crx_file::id_util::GenerateId("a");
  scoped_refptr<const extensions::Extension> unsupported_app =
      CreateAndInstallLockScreenAppWithPermissions(
          kUnsupportedAppId, kUnsupportedAppName, nullptr, profile());

  // Disabling preffered app on lock screen should fail if there is no preferred
  // app.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));

  helper()->SetPreferredApp(profile(), prod_app->id());

  // Setting preferred app should fire observers.
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Verify dev app is enabled on lock screen.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
                   false /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
                  {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
                   true /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
                  {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
                   NoteTakingLockScreenSupport::kNotSupported}}));

  // Allowlist prod app by policy.
  profile_prefs_->SetManagedPref(
      prefs::kNoteTakingAppsLockScreenAllowlist,
      extensions::ListBuilder().Append(prod_app->id()).Build());

  // The preferred app's status hasn't changed, so the observers can remain
  // agnostic of the policy change.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotAllowedByPolicy},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        NoteTakingLockScreenSupport::kNotSupported}}));

  // Change allowlist so only dev app is allowlisted.
  profile_prefs_->SetManagedPref(
      prefs::kNoteTakingAppsLockScreenAllowlist,
      extensions::ListBuilder().Append(dev_app->id()).Build());

  // The preferred app status changed, so observers are expected to be notified.
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Preferred app is not enabled on lock screen - chaning the lock screen
  // pref should fail.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kEnabled},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kNotAllowedByPolicy},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        NoteTakingLockScreenSupport::kNotSupported}}));

  // Switch preferred note taking app to one that does not support lock screen.
  helper()->SetPreferredApp(profile(), unsupported_app->id());

  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Policy with an empty allowlist - this should disallow all apps from the
  // lock screen.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 std::make_unique<base::ListValue>());

  // Preferred app changed notification is not expected if the preferred app is
  // not supported on lock screen.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotAllowedByPolicy},
       {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        false /*preferred*/, NoteTakingLockScreenSupport::kNotAllowedByPolicy},
       {kUnsupportedAppName, kUnsupportedAppId, true /*preferred*/,
        NoteTakingLockScreenSupport::kNotSupported}}));

  UninstallExtension(dev_app.get(), profile());
  UninstallExtension(prod_app.get(), profile());
  UninstallExtension(unsupported_app.get(), profile());

  profile_prefs_->RemoveManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist);
  // No preferred app installed, so no update notification.
  EXPECT_TRUE(observer.preferred_app_updates().empty());
}

TEST_F(NoteTakingHelperTest,
       UpdateLockScreenSupportStatusWhenAllowlistPolicyRemoved) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  // Add test app, set it as preferred and enable it on lock screen.
  scoped_refptr<const extensions::Extension> app =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());
  helper()->SetPreferredApp(profile(), app->id());
  observer.clear_preferred_app_updates();
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kEnabled}}));

  // Policy with an empty allowlist - this should disallow test app from running
  // on lock screen.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 std::make_unique<base::ListValue>());

  // Preferred app settings changed - observers should be notified.
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Verify the app is reported as not allowed by policy.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
                   true /*preferred*/,
                   NoteTakingLockScreenSupport::kNotAllowedByPolicy}}));

  // Remove the allowlist policy - the preferred app should become enabled on
  // lock screen again.
  profile_prefs_->RemoveManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist);

  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kEnabled}}));
}

TEST_F(NoteTakingHelperTest,
       NoObserverCallsIfPolicyChangesBeforeLockScreenStatusIsFetched) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  scoped_refptr<const extensions::Extension> app =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kDevKeepExtensionId,
                                    kDevKeepAppName, profile());

  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 std::make_unique<base::ListValue>());
  // Verify that observers are not notified of preferred app change if preferred
  // app is not set when allowlist policy changes.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Set test app as preferred note taking app.
  helper()->SetPreferredApp(profile(), app->id());
  EXPECT_EQ(std::vector<Profile*>{profile()}, observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Changing policy before the app's lock screen availability has been reported
  // to NoteTakingHelper clients is not expected to fire observers.
  profile_prefs_->SetManagedPref(
      prefs::kNoteTakingAppsLockScreenAllowlist,
      extensions::ListBuilder().Append(app->id()).Build());
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, NoteTakingHelper::kDevKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kEnabled}}));
}

TEST_F(NoteTakingHelperTest, LockScreenSupportInSecondaryProfile) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  // Initialize secondary profile.
  auto prefs = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());
  sync_preferences::TestingPrefServiceSyncable* profile_prefs = prefs.get();
  const std::string kSecondProfileName = "second-profile";
  TestingProfile* second_profile = profile_manager()->CreateTestingProfile(
      kSecondProfileName, std::move(prefs), u"Test profile", 1 /*avatar_id*/,
      std::string() /*supervised_user_id*/, TestingProfile::TestingFactories());
  InitExtensionService(second_profile);

  // Add test apps to secondary profile.
  scoped_refptr<const extensions::Extension> prod_app =
      CreateAndInstallLockScreenApp(NoteTakingHelper::kProdKeepExtensionId,
                                    kProdKeepAppName, second_profile);
  const std::string kUnsupportedAppName = "App name";
  const extensions::ExtensionId kUnsupportedAppId =
      crx_file::id_util::GenerateId("a");
  scoped_refptr<const extensions::Extension> unsupported_app =
      CreateAndInstallLockScreenAppWithPermissions(
          kUnsupportedAppId, kUnsupportedAppName, nullptr, second_profile);

  // Setting preferred app should fire observers for secondary profile.
  helper()->SetPreferredApp(second_profile, prod_app->id());
  EXPECT_EQ(std::vector<Profile*>{second_profile},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Even though prod app supports lock screen it should be reported as not
  // supported in the secondary profile.
  EXPECT_TRUE(AvailableAppsMatch(
      second_profile,
      {{kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        NoteTakingLockScreenSupport::kNotSupported}}));

  // Enabling an app on lock screen in secondary profile should fail.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));

  // Policy with an empty allowlist.
  profile_prefs->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                std::make_unique<base::ListValue>());

  // Changing policy should not notify observers in secondary profile.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      second_profile,
      {{kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
        true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        NoteTakingLockScreenSupport::kNotSupported}}));
  EXPECT_TRUE(PreferredAppMatches(
      second_profile,
      {kProdKeepAppName, NoteTakingHelper::kProdKeepExtensionId,
       true /*preferred*/, NoteTakingLockScreenSupport::kNotSupported}));
}

TEST_F(NoteTakingHelperTest, NoteTakingControllerClient) {
  Init(ENABLE_PALETTE);

  auto has_note_taking_apps = [&]() {
    auto* client = ash::NoteTakingClient::GetInstance();
    return client && client->CanCreateNote();
  };

  EXPECT_FALSE(has_note_taking_apps());

  SetNoteTakingClientProfile(profile());
  EXPECT_FALSE(has_note_taking_apps());

  scoped_refptr<const extensions::Extension> extension1 =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, kProdKeepAppName);
  scoped_refptr<const extensions::Extension> extension2 =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, kDevKeepAppName);

  InstallExtension(extension1.get(), profile());
  EXPECT_TRUE(has_note_taking_apps());

  InstallExtension(extension2.get(), profile());
  EXPECT_TRUE(has_note_taking_apps());

  UninstallExtension(extension1.get(), profile());
  EXPECT_TRUE(has_note_taking_apps());

  UninstallExtension(extension2.get(), profile());
  EXPECT_FALSE(has_note_taking_apps());

  InstallExtension(extension1.get(), profile());
  EXPECT_TRUE(has_note_taking_apps());

  const std::string kSecondProfileName = "second-profile";
  TestingProfile* second_profile =
      profile_manager()->CreateTestingProfile(kSecondProfileName);
  InitExtensionService(second_profile);

  SetNoteTakingClientProfile(second_profile);
  EXPECT_FALSE(has_note_taking_apps());

  InstallExtension(extension2.get(), second_profile);
  EXPECT_TRUE(has_note_taking_apps());

  SetNoteTakingClientProfile(profile());
  EXPECT_TRUE(has_note_taking_apps());

  ash::NoteTakingClient::GetInstance()->CreateNote();
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  ASSERT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);

  UninstallExtension(extension2.get(), second_profile);
  EXPECT_TRUE(has_note_taking_apps());

  profile_manager()->DeleteTestingProfile(kSecondProfileName);
}

}  // namespace chromeos

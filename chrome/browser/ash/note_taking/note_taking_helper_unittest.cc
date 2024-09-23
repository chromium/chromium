// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/note_taking/note_taking_helper.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/intent_common.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/note_taking_client.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/note_taking/note_taking_controller_client.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/display/test/display_manager_test_api.h"
#include "url/gurl.h"

namespace ash {

namespace app_runtime = extensions::api::app_runtime;

using ::arc::mojom::IntentHandlerInfo;
using ::arc::mojom::IntentHandlerInfoPtr;
using ::base::HistogramTester;
using HandledIntent = ::arc::FakeIntentHelperInstance::HandledIntent;
using LaunchResult = NoteTakingHelper::LaunchResult;

namespace {

constexpr LockScreenAppSupport kNotSupported =
    LockScreenAppSupport::kNotSupported;
constexpr LockScreenAppSupport kNotAllowedByPolicy =
    LockScreenAppSupport::kNotAllowedByPolicy;
constexpr LockScreenAppSupport kSupported = LockScreenAppSupport::kSupported;
constexpr LockScreenAppSupport kEnabled = LockScreenAppSupport::kEnabled;

auto& kDevKeepExtensionId = NoteTakingHelper::kDevKeepExtensionId;
auto& kProdKeepExtensionId = NoteTakingHelper::kProdKeepExtensionId;

// Name of default profile.
const char kTestProfileName[] = "test-profile";
const char kSecondProfileName[] = "second-profile";

// Names for keep apps used in tests.
const char kProdKeepAppName[] = "Google Keep [prod]";
const char kDevKeepAppName[] = "Google Keep [dev]";

// Helper functions returning strings that can be used to compare information
// about available note-taking apps.
std::string ToString(LockScreenAppSupport support) {
  std::ostringstream os;
  os << support;
  return os.str();
}
std::string GetAppString(const std::string& name,
                         const std::string& id,
                         bool preferred,
                         LockScreenAppSupport lock_screen_support) {
  return base::StringPrintf("{%s, %s, %d, %s}", name.c_str(), id.c_str(),
                            preferred, ToString(lock_screen_support).c_str());
}
std::string GetAppString(const NoteTakingAppInfo& info) {
  return GetAppString(info.name, info.app_id, info.preferred,
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

// Implementation of NoteTakingHelper::Observer for testing.
class TestObserver : public NoteTakingHelper::Observer {
 public:
  TestObserver() { NoteTakingHelper::Get()->AddObserver(this); }

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override { NoteTakingHelper::Get()->RemoveObserver(this); }

  int num_updates() const { return num_updates_; }
  void reset_num_updates() { num_updates_ = 0; }

  const std::vector<raw_ptr<Profile>> preferred_app_updates() const {
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
  std::vector<raw_ptr<Profile>> preferred_app_updates_;
};

}  // namespace

class NoteTakingHelperTest : public BrowserWithTestWindowTest {
 public:
  NoteTakingHelperTest() {
    // `media_router::kMediaRouter` is disabled because it has unmet
    // dependencies and is unrelated to this unit test.
    feature_list_.InitAndDisableFeature(media_router::kMediaRouter);
  }

  NoteTakingHelperTest(const NoteTakingHelperTest&) = delete;
  NoteTakingHelperTest& operator=(const NoteTakingHelperTest&) = delete;

  ~NoteTakingHelperTest() override = default;

  void SetUp() override {
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(true);
    SessionManagerClient::InitializeFakeInMemory();
    FakeSessionManagerClient::Get()->set_arc_available(true);

    BrowserWithTestWindowTest::SetUp();
    InitExtensionService(profile());
    InitWebAppProvider(profile());
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
      intent_helper_host_.reset();
      file_system_bridge_.reset();
      arc_test_.TearDown();
    }
    extensions::ExtensionSystem::Get(profile())->Shutdown();
    BrowserWithTestWindowTest::TearDown();
    SessionManagerClient::Shutdown();
    ash::ProfileHelper::SetProfileToUserForTestingEnabled(false);
  }

 protected:
  // Information about a Chrome app passed to LaunchChromeApp().
  struct ChromeAppLaunchInfo {
    extensions::ExtensionId id;
  };

  // Options that can be passed to Init().
  enum InitFlags : uint32_t {
    ENABLE_PLAY_STORE = 1 << 0,
    ENABLE_PALETTE = 1 << 1,
  };

  static NoteTakingHelper* helper() { return NoteTakingHelper::Get(); }

  LockScreenAppSupport GetLockScreenSupport(Profile* profile,
                                            const std::string& app_id) {
    return LockScreenApps::GetSupport(profile, app_id);
  }

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
    // Set up FakeIntentHelperHost to emulate full-duplex IntentHelper
    // connection.
    intent_helper_host_ = std::make_unique<arc::FakeIntentHelperHost>(
        arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper());
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
          switches::kAshForceEnableStylusTools);
    }

    // TODO(derat): Sigh, something in ArcAppTest appears to be re-enabling ARC.
    profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled,
                                      flags & ENABLE_PLAY_STORE);
    NoteTakingHelper::Initialize();
    NoteTakingHelper::Get()->set_launch_chrome_app_callback_for_test(
        base::BindRepeating(&NoteTakingHelperTest::LaunchChromeApp,
                            base::Unretained(this)));
  }

  // Creates an extension.
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name) {
    return CreateExtension(id, name, std::nullopt, std::nullopt);
  }
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name,
      std::optional<base::Value::List> permissions,
      std::optional<base::Value::List> action_handlers) {
    base::Value::Dict manifest =
        base::Value::Dict()
            .Set("name", name)
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("app", base::Value::Dict().Set(
                            "background",
                            base::Value::Dict().Set(
                                "scripts",
                                base::Value::List().Append("background.js"))));

    if (action_handlers)
      manifest.Set("action_handlers", std::move(*action_handlers));

    if (permissions)
      manifest.Set("permissions", std::move(*permissions));

    return extensions::ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(id)
        .Build();
  }

  void InitWebAppProvider(Profile* profile) {
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile);
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
        id, app_name, base::Value::List().Append("lockScreen"), profile);
  }

  scoped_refptr<const extensions::Extension>
  CreateAndInstallLockScreenAppWithPermissions(
      const std::string& id,
      const std::string& app_name,
      std::optional<base::Value::List> permissions,
      Profile* profile) {
    base::Value::List lock_enabled_action_handler = base::Value::List().Append(
        base::Value::Dict()
            .Set("action",
                 app_runtime::ToString(app_runtime::ActionType::kNewNote))
            .Set("enabled_on_lock_screen", true));

    scoped_refptr<const extensions::Extension> keep_extension =
        CreateExtension(id, app_name, std::move(permissions),
                        std::move(lock_enabled_action_handler));
    InstallExtension(keep_extension.get(), profile);

    return keep_extension;
  }

  // BrowserWithTestWindowTest:
  std::string GetDefaultProfileName() override { return kTestProfileName; }

  // TODO(crbug.com/40286020): merge into BrowserWithTestWindowTest.
  void LogIn(const std::string& email) override {
    AccountId account_id = AccountId::FromUserEmail(email);
    user_manager()->AddUser(account_id);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    profile_prefs_ = prefs.get();
    auto* profile = profile_manager()->CreateTestingProfile(
        profile_name, std::move(prefs), u"Test profile", 1 /*avatar_id*/,
        TestingProfile::TestingFactories());
    OnUserProfileCreated(profile_name, profile);
    return profile;
  }

  TestingProfile* CreateAndInitSecondaryProfile() {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    const AccountId account_id(AccountId::FromUserEmail(kSecondProfileName));
    user_manager()->AddUser(account_id);
    TestingProfile* profile = profile_manager()->CreateTestingProfile(
        kSecondProfileName, std::move(prefs), u"second-profile-username",
        /*avatar_id=*/1, TestingProfile::TestingFactories());
    OnUserProfileCreated(kSecondProfileName, profile);

    InitExtensionService(profile);
    InitWebAppProvider(profile);
    DCHECK(!ash::ProfileHelper::IsPrimaryProfile(profile));

    return profile;
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
  raw_ptr<sync_preferences::TestingPrefServiceSyncable, DanglingUntriaged>
      profile_prefs_ = nullptr;

 private:
  // Callback registered with the helper to record Chrome app launch requests.
  void LaunchChromeApp(content::BrowserContext* passed_context,
                       const extensions::Extension* extension,
                       app_runtime::ActionData action_data) {
    EXPECT_EQ(profile(), passed_context);
    EXPECT_EQ(app_runtime::ActionType::kNewNote, action_data.action_type);
    launched_chrome_apps_.push_back(ChromeAppLaunchInfo{extension->id()});
  }

  // Has Init() been called?
  bool initialized_ = false;

  ArcAppTest arc_test_{ArcAppTest::UserManagerMode::kDoNothing};
  std::unique_ptr<arc::FakeIntentHelperHost> intent_helper_host_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NoteTakingHelperTest, PaletteNotEnabled) {
  // Without the palette enabled, IsAppAvailable() should return false.
  Init(0);
  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(kProdKeepExtensionId, "Keep");
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
      CreateExtension(kProdKeepExtensionId, kProdKeepAppName);
  InstallExtension(prod_extension.get(), profile());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kProdKeepAppName, kProdKeepExtensionId,
                                      false /*preferred*/, kNotSupported}}));

  // If the dev version is also installed, it should be listed before the prod
  // version.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateExtension(kDevKeepExtensionId, kDevKeepAppName);
  InstallExtension(dev_extension.get(), profile());
  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kDevKeepAppName, kDevKeepExtensionId,
                                      false /*preferred*/, kNotSupported},
                                     {kProdKeepAppName, kProdKeepExtensionId,
                                      false /*preferred*/, kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredAppId(profile()).empty());

  // Now install a random web app to check that it's ignored.
  web_app::test::InstallDummyWebApp(profile(), "Web App",
                                    GURL("http://some.url"));
  // Now install a random extension to check that it's ignored.
  const extensions::ExtensionId kOtherId = crx_file::id_util::GenerateId("a");
  const std::string kOtherName = "Some Other App";
  scoped_refptr<const extensions::Extension> other_extension =
      CreateExtension(kOtherId, kOtherName);
  InstallExtension(other_extension.get(), profile());

  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kDevKeepAppName, kDevKeepExtensionId,
                                      false /*preferred*/, kNotSupported},
                                     {kProdKeepAppName, kProdKeepExtensionId,
                                      false /*preferred*/, kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredAppId(profile()).empty());

  // Mark the prod version as preferred.
  helper()->SetPreferredApp(profile(), kProdKeepExtensionId);
  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kDevKeepAppName, kDevKeepExtensionId,
                                      false /*preferred*/, kNotSupported},
                                     {kProdKeepAppName, kProdKeepExtensionId,
                                      true /*preferred*/, kNotSupported}}));
  EXPECT_EQ(helper()->GetPreferredAppId(profile()), kProdKeepExtensionId);
  EXPECT_EQ(GetLockScreenSupport(profile(), kProdKeepExtensionId),
            kNotSupported);
}

TEST_F(NoteTakingHelperTest, ListChromeAppsWithLockScreenNotesSupported) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  base::Value::List lock_disabled_action_handler = base::Value::List().Append(
      app_runtime::ToString(app_runtime::ActionType::kNewNote));

  // Install Keep app that does not support lock screen note taking - it should
  // be reported not to support lock screen note taking.
  scoped_refptr<const extensions::Extension> prod_extension = CreateExtension(
      kProdKeepExtensionId, kProdKeepAppName, /*permissions=*/std::nullopt,
      std::move(lock_disabled_action_handler));
  InstallExtension(prod_extension.get(), profile());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kProdKeepAppName, kProdKeepExtensionId,
                                      false /*preferred*/, kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredAppId(profile()).empty());

  // Install additional Keep app - one that supports lock screen note taking.
  // This app should be reported to support note taking.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/, kEnabled},
       {kProdKeepAppName, kProdKeepExtensionId, false /*preferred*/,
        kNotSupported}}));
  EXPECT_TRUE(helper()->GetPreferredAppId(profile()).empty());
}

TEST_F(NoteTakingHelperTest, PreferredAppEnabledOnLockScreen) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // Install lock screen enabled Keep note taking app.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());

  // Verify that the app is reported to support lock screen note taking.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/, kEnabled}}));

  // When the lock screen note taking pref is set and the Keep app is set as the
  // preferred note taking app, the app should be reported as selected as lock
  // screen note taking app.
  helper()->SetPreferredApp(profile(), kDevKeepExtensionId);
  helper()->SetPreferredAppEnabledOnLockScreen(profile(), true);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, true /*preferred*/, kEnabled}}));
  EXPECT_EQ(helper()->GetPreferredAppId(profile()), kDevKeepExtensionId);
  EXPECT_EQ(GetLockScreenSupport(profile(), kDevKeepExtensionId), kEnabled);

  helper()->SetPreferredAppEnabledOnLockScreen(profile(), false);

  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kDevKeepAppName, kDevKeepExtensionId,
                                      true /*preferred*/, kSupported}}));
  EXPECT_EQ(helper()->GetPreferredAppId(profile()), kDevKeepExtensionId);
  EXPECT_EQ(GetLockScreenSupport(profile(), kDevKeepExtensionId), kSupported);
}

TEST_F(NoteTakingHelperTest, PreferredAppWithNoLockScreenPermission) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // Install lock screen enabled Keep note taking app, but wihtout lock screen
  // permission listed.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenAppWithPermissions(
          kDevKeepExtensionId, kDevKeepAppName, /*permissions=*/std::nullopt,
          profile());
  helper()->SetPreferredApp(profile(), kDevKeepExtensionId);

  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kDevKeepAppName, kDevKeepExtensionId,
                                      true /*preferred*/, kNotSupported}}));
  EXPECT_EQ(helper()->GetPreferredAppId(profile()), kDevKeepExtensionId);
  EXPECT_EQ(GetLockScreenSupport(profile(), kDevKeepExtensionId),
            kNotSupported);
}

TEST_F(NoteTakingHelperTest,
       PreferredAppWithoutLockSupportClearsLockScreenPref) {
  Init(ENABLE_PALETTE);

  ASSERT_FALSE(helper()->IsAppAvailable(profile()));
  ASSERT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // Install dev Keep app that supports lock screen note taking.
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());

  // Install third-party app that doesn't support lock screen note taking.
  const extensions::ExtensionId kNewNoteId = crx_file::id_util::GenerateId("a");
  const std::string kName = "Some App";
  scoped_refptr<const extensions::Extension> has_new_note =
      CreateAndInstallLockScreenAppWithPermissions(
          kNewNoteId, kName, /*permissions=*/std::nullopt, profile());

  // Verify that only Keep app is reported to support lock screen note taking.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/, kEnabled},
       {kName, kNewNoteId, false /*preferred*/, kNotSupported}}));

  // When the Keep app is set as preferred app, and note taking on lock screen
  // is enabled, the keep app should be reported to be selected as the lock
  // screen note taking app.
  helper()->SetPreferredApp(profile(), kDevKeepExtensionId);
  helper()->SetPreferredAppEnabledOnLockScreen(profile(), true);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, true /*preferred*/, kEnabled},
       {kName, kNewNoteId, false /*preferred*/, kNotSupported}}));

  // When a third party app (which does not support lock screen note taking) is
  // set as the preferred app, Keep app's lock screen support state remain
  // enabled - even though it will not be launchable from the lock screen.
  helper()->SetPreferredApp(profile(), kNewNoteId);
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/, kEnabled},
       {kName, kNewNoteId, true /*preferred*/, kNotSupported}}));
  EXPECT_EQ(helper()->GetPreferredAppId(profile()), kNewNoteId);
  EXPECT_EQ(GetLockScreenSupport(profile(), kNewNoteId), kNotSupported);
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
      kNewNoteId, kName, /*permissions=*/std::nullopt,
      base::Value::List().Append(
          app_runtime::ToString(app_runtime::ActionType::kNewNote)));
  InstallExtension(has_new_note.get(), profile());
  // "action_handlers": []
  scoped_refptr<const extensions::Extension> empty_array = CreateExtension(
      kEmptyArrayId, kName, /*permissions=*/std::nullopt, base::Value::List());
  InstallExtension(empty_array.get(), profile());
  // (no action handler entry)
  scoped_refptr<const extensions::Extension> none =
      CreateExtension(kEmptyId, kName);
  InstallExtension(none.get(), profile());

  // Only the "new_note" extension is returned from GetAvailableApps.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName, kNewNoteId, false /*preferred*/, kNotSupported}}));
}

// Web apps with a note_taking_new_note_url show as available note-taking apps.
TEST_F(NoteTakingHelperTest, NoteTakingWebAppsListed) {
  Init(ENABLE_PALETTE);

  {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("http://some1.url"));
    app_info->scope = GURL("http://some1.url");
    app_info->title = u"Web App 1";
    web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  std::string app2_id;
  {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("http://some2.url"));
    app_info->scope = GURL("http://some2.url");
    app_info->title = u"Web App 2";
    // Set a note_taking_new_note_url on one app.
    app_info->note_taking_new_note_url = GURL("http://some2.url/new-note");
    app2_id = web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  // Check apps were installed.
  auto* provider = web_app::WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().CountUserInstalledApps(), 2);

  // Apps with note_taking_new_note_url are listed.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{"Web App 2", app2_id, false /*preferred*/, kNotSupported}}));
}

// Web apps with a lock_screen_start_url should show as supported on the lock
// screen only when `kWebLockScreenApi` is enabled.
// TODO(crbug.com/40227659): Move this to a lock screen apps unittest file.
TEST_F(NoteTakingHelperTest, LockScreenWebAppsListed) {
  Init(ENABLE_PALETTE);
  DCHECK(!base::FeatureList::IsEnabled(::features::kWebLockScreenApi));

  std::string app1_id;
  {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("http://some1.url"));
    app_info->scope = GURL("http://some1.url");
    app_info->title = u"Web App 1";
    // Currently only note-taking apps can be used on the lock screen.
    app_info->note_taking_new_note_url = GURL("http://some2.url/new-note");
    app1_id = web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  std::string app2_id;
  {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("http://some2.url"));
    app_info->scope = GURL("http://some2.url");
    app_info->title = u"Web App 2";
    app_info->note_taking_new_note_url = GURL("http://some2.url/new-note");
    // Set a lock_screen_start_url on one app.
    app_info->lock_screen_start_url =
        GURL("http://some2.url/lock-screen-start");
    app2_id = web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  // Check apps were installed.
  auto* provider = web_app::WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().CountUserInstalledApps(), 2);

  // With the flag disabled, web apps are not supported.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{"Web App 1", app1_id, /*preferred=*/false, kNotSupported},
                  {"Web App 2", app2_id, /*preferred=*/false, kNotSupported}}));
}

class NoteTakingHelperTest_WebLockScreenApiEnabled
    : public NoteTakingHelperTest {
  base::test::ScopedFeatureList features_{::features::kWebLockScreenApi};
};

// Web apps with a lock_screen_start_url should show as supported on the lock
// screen only when `kWebLockScreenApi` is enabled.
// TODO(crbug.com/40227659): Move this to a lock screen apps unittest file.
TEST_F(NoteTakingHelperTest_WebLockScreenApiEnabled, LockScreenWebAppsListed) {
  Init(ENABLE_PALETTE);
  DCHECK(base::FeatureList::IsEnabled(::features::kWebLockScreenApi));

  std::string app1_id;
  {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("http://some1.url"));
    app_info->scope = GURL("http://some1.url");
    app_info->title = u"Web App 1";
    // Currently only note-taking apps can be used on the lock screen.
    app_info->note_taking_new_note_url = GURL("http://some2.url/new-note");
    app1_id = web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  std::string app2_id;
  {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("http://some2.url"));
    app_info->scope = GURL("http://some2.url");
    app_info->title = u"Web App 2";
    app_info->note_taking_new_note_url = GURL("http://some2.url/new-note");
    // Set a lock_screen_start_url on one app.
    app_info->lock_screen_start_url =
        GURL("http://some2.url/lock-screen-start");
    app2_id = web_app::test::InstallWebApp(profile(), std::move(app_info));
  }
  // Check apps were installed.
  auto* provider = web_app::WebAppProvider::GetForTest(profile());
  EXPECT_EQ(provider->registrar_unsafe().CountUserInstalledApps(), 2);

  // The web app with a lock screen start URL is supported.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{"Web App 1", app1_id, /*preferred=*/false, kNotSupported},
                  {"Web App 2", app2_id, /*preferred=*/false, kEnabled}}));
}

// Verify that non-allowlisted apps cannot be enabled on lock screen.
TEST_F(NoteTakingHelperTest, CustomLockScreenEnabledApps) {
  Init(ENABLE_PALETTE);

  const extensions::ExtensionId kNewNoteId = crx_file::id_util::GenerateId("a");
  const std::string kName = "Some App";

  scoped_refptr<const extensions::Extension> extension =
      CreateAndInstallLockScreenApp(kNewNoteId, kName, profile());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName, kNewNoteId, false /*preferred*/, kNotSupported}}));
}

TEST_F(NoteTakingHelperTest, AllowlistedAndCustomAppsShowOnlyOnce) {
  Init(ENABLE_PALETTE);

  scoped_refptr<const extensions::Extension> extension = CreateExtension(
      kProdKeepExtensionId, "Keep", /*permissions=*/std::nullopt,
      base::Value::List().Append(
          app_runtime::ToString(app_runtime::ActionType::kNewNote)));
  InstallExtension(extension.get(), profile());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{"Keep", kProdKeepExtensionId, false /*preferred*/, kNotSupported}}));
}

TEST_F(NoteTakingHelperTest, LaunchChromeApp) {
  Init(ENABLE_PALETTE);
  scoped_refptr<const extensions::Extension> extension =
      CreateExtension(kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get(), profile());

  // Check the Chrome app is launched with the correct parameters.
  HistogramTester histogram_tester;
  helper()->LaunchAppForNewNote(profile());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(kProdKeepExtensionId, launched_chrome_apps_[0].id);

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
}

TEST_F(NoteTakingHelperTest, FallBackIfPreferredAppUnavailable) {
  Init(ENABLE_PALETTE);
  scoped_refptr<const extensions::Extension> prod_extension =
      CreateExtension(kProdKeepExtensionId, "prod");
  InstallExtension(prod_extension.get(), profile());
  scoped_refptr<const extensions::Extension> dev_extension =
      CreateExtension(kDevKeepExtensionId, "dev");
  InstallExtension(dev_extension.get(), profile());
  {
    // Install a default-allowed web app corresponding to ID of
    // |NoteTakingHelper::kNoteTakingWebAppIdTest|.
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://yielding-large-chef.glitch.me/"));
    app_info->title = u"Default Allowed Web App";
    std::string app_id =
        web_app::test::InstallWebApp(profile(), std::move(app_info));
    EXPECT_EQ(app_id, NoteTakingHelper::kNoteTakingWebAppIdTest);
  }

  // Set the prod app as preferred and check that it's launched.
  std::unique_ptr<HistogramTester> histogram_tester(new HistogramTester());
  helper()->SetPreferredApp(profile(), kProdKeepExtensionId);
  helper()->LaunchAppForNewNote(profile());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  ASSERT_EQ(kProdKeepExtensionId, launched_chrome_apps_[0].id);

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
  histogram_tester->ExpectTotalCount(
      NoteTakingHelper::kDefaultLaunchResultHistogramName, 0);

  // Now uninstall the prod app and check that we fall back to the dev app.
  UninstallExtension(prod_extension.get(), profile());
  launched_chrome_apps_.clear();
  histogram_tester = std::make_unique<HistogramTester>();
  helper()->LaunchAppForNewNote(profile());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(kDevKeepExtensionId, launched_chrome_apps_[0].id);

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_APP_MISSING), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);

  // Now uninstall the dev app and check that we fall back to the test web app.
  UninstallExtension(dev_extension.get(), profile());
  launched_chrome_apps_.clear();
  histogram_tester = std::make_unique<HistogramTester>();
  helper()->LaunchAppForNewNote(profile());
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
  helper()->OnIntentFiltersUpdated(std::nullopt);
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
  auto prefs = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());
  prefs->SetBoolean(arc::prefs::kArcEnabled, true);
  profile_manager()->CreateTestingProfile(kSecondProfileName, std::move(prefs),
                                          u"Second User", 1 /* avatar_id */,
                                          TestingProfile::TestingFactories());
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_EQ(1, observer.num_updates());

  // TODO(derat|hidehiko): Check that NoteTakingHelper adds itself as an
  // observer of the ArcIntentHelperBridge corresponding to the new profile:
  // https://crbug.com/748763

  // Notification of updated intent filters should result in the apps being
  // refreshed.
  helper()->OnIntentFiltersUpdated(std::nullopt);
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
      profile(), {{kName1, kPackage1, false /*preferred*/, kNotSupported},
                  {kName2, kPackage2, false /*preferred*/, kNotSupported}}));

  helper()->SetPreferredApp(profile(), kPackage1);

  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kName1, kPackage1, true /*preferred*/, kNotSupported},
                  {kName2, kPackage2, false /*preferred*/, kNotSupported}}));
  // Preferred app is not actually installed, so no app ID should be returned.
  EXPECT_TRUE(helper()->GetPreferredAppId(profile()).empty());
  EXPECT_EQ(GetLockScreenSupport(profile(), ""), kNotSupported);

  // Disable Play Store and check that the apps are no longer returned.
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());
}

TEST_F(NoteTakingHelperTest, LaunchAndroidAppNoDisplay) {
  // Opening Android apps via OpenUrlsWithPermissionAndWindowInfo requires a
  // valid internal display, not being able to find one will halt launch.
  const std::string kPackage1 = "org.chromium.package1";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App 1", kPackage1));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper()->IsAppAvailable(profile()));

  // The installed app fails to launch, registering on histogram.
  std::unique_ptr<HistogramTester> histogram_tester(new HistogramTester());
  helper()->LaunchAppForNewNote(profile());
  ASSERT_EQ(0u, file_system_->handledUrlRequests().size());

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_INTERNAL_DISPLAY_FOUND), 1);
}

TEST_F(NoteTakingHelperTest, LaunchAndroidApp) {
  // Since now launching Android apps require window info, this step is needed
  // to make display info available.
  ASSERT_TRUE(Shell::Get());
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();

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
  helper()->LaunchAppForNewNote(profile());
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
  helper()->OnIntentFiltersUpdated(std::nullopt);
  base::RunLoop().RunUntilIdle();
  helper()->SetPreferredApp(profile(), kPackage2);

  // The second app should be launched now.
  intent_helper_.clear_handled_intents();
  file_system_->clear_handled_requests();
  histogram_tester = std::make_unique<HistogramTester>();
  helper()->LaunchAppForNewNote(profile());
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

TEST_F(NoteTakingHelperTest, NoAppsAvailable) {
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);

  // When no note-taking apps are installed, the histograms should just be
  // updated.
  HistogramTester histogram_tester;
  helper()->LaunchAppForNewNote(profile());
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
  helper()->OnIntentFiltersUpdated(std::nullopt);
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
      CreateExtension(kProdKeepExtensionId, "Keep");
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
  TestingProfile* second_profile = CreateAndInitSecondaryProfile();
  DCHECK(ash::ProfileHelper::IsPrimaryProfile(profile()));
  DCHECK(!ash::ProfileHelper::IsPrimaryProfile(second_profile));

  scoped_refptr<const extensions::Extension> second_keep_extension =
      CreateExtension(kProdKeepExtensionId, "Keep");
  EXPECT_EQ(0, observer.num_updates());
  InstallExtension(second_keep_extension.get(), second_profile);
  EXPECT_EQ(1, observer.num_updates());
  UninstallExtension(second_keep_extension.get(), second_profile);
  EXPECT_EQ(2, observer.num_updates());
  profile_manager()->DeleteTestingProfile(kSecondProfileName);
}

TEST_F(NoteTakingHelperTest, NotifyObserverAboutPreferredAppChanges) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  scoped_refptr<const extensions::Extension> prod_keep_extension =
      CreateExtension(kProdKeepExtensionId, "Keep");
  InstallExtension(prod_keep_extension.get(), profile());

  scoped_refptr<const extensions::Extension> dev_keep_extension =
      CreateExtension(kDevKeepExtensionId, "Keep");
  InstallExtension(dev_keep_extension.get(), profile());

  ASSERT_TRUE(observer.preferred_app_updates().empty());

  // Observers should be notified when preferred app is set.
  helper()->SetPreferredApp(profile(), prod_keep_extension->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // If the preferred app is not changed, observers should not be notified.
  helper()->SetPreferredApp(profile(), prod_keep_extension->id());
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Observers should be notified when preferred app is changed.
  helper()->SetPreferredApp(profile(), dev_keep_extension->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Observers should be notified when preferred app is cleared.
  helper()->SetPreferredApp(profile(), "");
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // No change to preferred app.
  helper()->SetPreferredApp(profile(), "");
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Initialize secondary profile with a test app.
  TestingProfile* second_profile = CreateAndInitSecondaryProfile();
  scoped_refptr<const extensions::Extension>
      second_profile_prod_keep_extension =
          CreateExtension(kProdKeepExtensionId, "Keep");
  InstallExtension(second_profile_prod_keep_extension.get(), second_profile);

  // Verify that observers are called with the scondary profile if the secondary
  // profile preferred app changes.
  helper()->SetPreferredApp(second_profile,
                            second_profile_prod_keep_extension->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{second_profile},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Clearing preferred app in secondary ptofile should fire observers with the
  // secondary profile.
  helper()->SetPreferredApp(second_profile, "");
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{second_profile},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  profile_manager()->DeleteTestingProfile(kSecondProfileName);
}

TEST_F(NoteTakingHelperTest,
       NotifyObserverAboutPreferredLockScreenAppSupportChanges) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  scoped_refptr<const extensions::Extension> dev_extension =
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());

  scoped_refptr<const extensions::Extension> prod_extension =
      CreateExtension(kProdKeepExtensionId, "Keep");
  InstallExtension(prod_extension.get(), profile());

  ASSERT_TRUE(observer.preferred_app_updates().empty());

  // Set the app that supports lock screen note taking as preferred.
  helper()->SetPreferredApp(profile(), dev_extension->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Disable the preferred app on the lock screen.
  EXPECT_TRUE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Disabling lock screen support for already enabled app should be no-op.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Change the state of the preferred app - it should succeed, and a
  // notification should be fired.
  EXPECT_TRUE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // No-op, because the preferred app state is not changing.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Set an app that does not support lock screen as primary.
  helper()->SetPreferredApp(profile(), prod_extension->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
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
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());
  scoped_refptr<const extensions::Extension> prod_app =
      CreateAndInstallLockScreenApp(kProdKeepExtensionId, kProdKeepAppName,
                                    profile());
  const std::string kUnsupportedAppName = "App name";
  const extensions::ExtensionId kUnsupportedAppId =
      crx_file::id_util::GenerateId("a");
  scoped_refptr<const extensions::Extension> unsupported_app =
      CreateAndInstallLockScreenAppWithPermissions(
          kUnsupportedAppId, kUnsupportedAppName, /*permissions=*/std::nullopt,
          profile());

  // Disabling preferred app on lock screen should fail if there is no preferred
  // app.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));

  helper()->SetPreferredApp(profile(), prod_app->id());

  // Setting preferred app should fire observers.
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Verify dev and prod apps are enabled for lock screen, with prod preferred.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/, kEnabled},
       {kProdKeepAppName, kProdKeepExtensionId, true /*preferred*/, kEnabled},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        kNotSupported}}));

  // Allowlist prod app by policy.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 base::Value::List().Append(prod_app->id()));

  // The preferred app's status hasn't changed, so the observers can remain
  // agnostic of the policy change.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/,
        kNotAllowedByPolicy},
       {kProdKeepAppName, kProdKeepExtensionId, true /*preferred*/, kEnabled},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        kNotSupported}}));

  // Change allowlist so only dev app is allowlisted.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 base::Value::List().Append(dev_app->id()));

  // The preferred app status changed, so observers are expected to be notified.
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Preferred app is not enabled on lock screen - chaning the lock screen
  // pref should fail.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), false));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, false /*preferred*/, kEnabled},
       {kProdKeepAppName, kProdKeepExtensionId, true /*preferred*/,
        kNotAllowedByPolicy},
       {kUnsupportedAppName, kUnsupportedAppId, false /*preferred*/,
        kNotSupported}}));

  // Switch preferred note taking app to one that does not support lock screen.
  helper()->SetPreferredApp(profile(), unsupported_app->id());

  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Policy with an empty allowlist - this should disallow all apps from the
  // lock screen.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 base::Value::List());

  // Preferred app changed notification is not expected if the preferred app is
  // not supported on lock screen.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(
      AvailableAppsMatch(profile(), {{kDevKeepAppName, kDevKeepExtensionId,
                                      false /*preferred*/, kNotAllowedByPolicy},
                                     {kProdKeepAppName, kProdKeepExtensionId,
                                      false /*preferred*/, kNotAllowedByPolicy},
                                     {kUnsupportedAppName, kUnsupportedAppId,
                                      true /*preferred*/, kNotSupported}}));

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
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());
  helper()->SetPreferredApp(profile(), app->id());
  observer.clear_preferred_app_updates();
  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, true /*preferred*/, kEnabled}}));

  // Policy with an empty allowlist - this should disallow test app from running
  // on lock screen.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 base::Value::List());

  // Preferred app settings changed - observers should be notified.
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Verify the app is reported as not allowed by policy.
  EXPECT_TRUE(AvailableAppsMatch(
      profile(), {{kDevKeepAppName, kDevKeepExtensionId, true /*preferred*/,
                   kNotAllowedByPolicy}}));

  // Remove the allowlist policy - the preferred app should become enabled on
  // lock screen again.
  profile_prefs_->RemoveManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist);

  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, true /*preferred*/, kEnabled}}));
}

TEST_F(NoteTakingHelperTest,
       NoObserverCallsIfPolicyChangesBeforeLockScreenStatusIsFetched) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  scoped_refptr<const extensions::Extension> app =
      CreateAndInstallLockScreenApp(kDevKeepExtensionId, kDevKeepAppName,
                                    profile());

  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 base::Value::List());
  // Verify that observers are not notified of preferred app change if preferred
  // app is not set when allowlist policy changes.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  // Set test app as preferred note taking app.
  helper()->SetPreferredApp(profile(), app->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{profile()},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Changing policy before the app's lock screen availability has been reported
  // to NoteTakingHelper clients is not expected to fire observers.
  profile_prefs_->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                 base::Value::List().Append(app->id()));
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(
      profile(),
      {{kDevKeepAppName, kDevKeepExtensionId, true /*preferred*/, kEnabled}}));
}

TEST_F(NoteTakingHelperTest, LockScreenSupportInSecondaryProfile) {
  Init(ENABLE_PALETTE);
  TestObserver observer;

  TestingProfile* second_profile = CreateAndInitSecondaryProfile();

  // Add test apps to secondary profile.
  scoped_refptr<const extensions::Extension> prod_app =
      CreateAndInstallLockScreenApp(kProdKeepExtensionId, kProdKeepAppName,
                                    second_profile);
  const std::string kUnsupportedAppName = "App name";
  const extensions::ExtensionId kUnsupportedAppId =
      crx_file::id_util::GenerateId("a");
  scoped_refptr<const extensions::Extension> unsupported_app =
      CreateAndInstallLockScreenAppWithPermissions(
          kUnsupportedAppId, kUnsupportedAppName, /*permissions=*/std::nullopt,
          second_profile);

  // Setting preferred app should fire observers for secondary profile.
  helper()->SetPreferredApp(second_profile, prod_app->id());
  EXPECT_EQ(std::vector<raw_ptr<Profile>>{second_profile},
            observer.preferred_app_updates());
  observer.clear_preferred_app_updates();

  // Even though prod app supports lock screen it should be reported as not
  // supported in the secondary profile.
  EXPECT_TRUE(AvailableAppsMatch(second_profile,
                                 {{kProdKeepAppName, kProdKeepExtensionId,
                                   true /*preferred*/, kNotSupported},
                                  {kUnsupportedAppName, kUnsupportedAppId,
                                   false /*preferred*/, kNotSupported}}));

  // Enabling an app on lock screen in secondary profile should fail.
  EXPECT_FALSE(helper()->SetPreferredAppEnabledOnLockScreen(profile(), true));

  auto* profile_prefs =
      static_cast<sync_preferences::TestingPrefServiceSyncable*>(
          second_profile->GetPrefs());
  // Policy with an empty allowlist.
  profile_prefs->SetManagedPref(prefs::kNoteTakingAppsLockScreenAllowlist,
                                base::Value::List());

  // Changing policy should not notify observers in secondary profile.
  EXPECT_TRUE(observer.preferred_app_updates().empty());

  EXPECT_TRUE(AvailableAppsMatch(second_profile,
                                 {{kProdKeepAppName, kProdKeepExtensionId,
                                   true /*preferred*/, kNotSupported},
                                  {kUnsupportedAppName, kUnsupportedAppId,
                                   false /*preferred*/, kNotSupported}}));
  EXPECT_EQ(helper()->GetPreferredAppId(second_profile), kProdKeepExtensionId);
  EXPECT_EQ(GetLockScreenSupport(second_profile, kProdKeepExtensionId),
            kNotSupported);
}

TEST_F(NoteTakingHelperTest, NoteTakingControllerClient) {
  Init(ENABLE_PALETTE);

  auto has_note_taking_apps = [&]() {
    auto* client = NoteTakingClient::GetInstance();
    return client && client->CanCreateNote();
  };

  EXPECT_FALSE(has_note_taking_apps());

  {
    SetNoteTakingClientProfile(profile());
    EXPECT_FALSE(has_note_taking_apps());

    scoped_refptr<const extensions::Extension> extension1 =
        CreateExtension(kProdKeepExtensionId, kProdKeepAppName);
    scoped_refptr<const extensions::Extension> extension2 =
        CreateExtension(kDevKeepExtensionId, kDevKeepAppName);

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
  }

  {
    TestingProfile* second_profile = CreateAndInitSecondaryProfile();

    SetNoteTakingClientProfile(second_profile);
    EXPECT_FALSE(has_note_taking_apps());

    scoped_refptr<const extensions::Extension> extension1 =
        CreateExtension(kProdKeepExtensionId, kProdKeepAppName);
    scoped_refptr<const extensions::Extension> extension2 =
        CreateExtension(kDevKeepExtensionId, kDevKeepAppName);

    InstallExtension(extension2.get(), second_profile);
    EXPECT_TRUE(has_note_taking_apps());

    SetNoteTakingClientProfile(profile());
    EXPECT_TRUE(has_note_taking_apps());

    NoteTakingClient::GetInstance()->CreateNote();
    ASSERT_EQ(1u, launched_chrome_apps_.size());
    ASSERT_EQ(kProdKeepExtensionId, launched_chrome_apps_[0].id);

    UninstallExtension(extension2.get(), second_profile);
    EXPECT_TRUE(has_note_taking_apps());

    profile_manager()->DeleteTestingProfile(kSecondProfileName);
  }
}

}  // namespace ash

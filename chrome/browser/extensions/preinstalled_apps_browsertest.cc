// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include "base/json/json_reader.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/web_applications/components/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_utils.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_installation_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"

namespace extensions {

namespace {

constexpr const char kDefaultInstalledId[] = "kbmnembihfiondgfjekmnmcbddelicoi";

constexpr const char kTestAppConfig[] =
    R"({
         "app_url": "%s",
         "launch_container": "window",
         "user_type": ["unmanaged"],
         "feature_name": "MigrateDefaultChromeAppToWebAppsNonGSuite",
         "uninstall_and_replace": ["kbmnembihfiondgfjekmnmcbddelicoi"]
       })";

base::FilePath GetTestPreinstalledAppsDir() {
  base::FilePath path;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions/test_default_apps");
  return path;
}

}  // namespace

class PreinstalledAppsBrowserTest : public ExtensionBrowserTest {
 public:
  PreinstalledAppsBrowserTest()
      : preinstalled_apps_(chrome::DIR_DEFAULT_APPS,
                           GetTestPreinstalledAppsDir()) {}
  PreinstalledAppsBrowserTest(const PreinstalledAppsBrowserTest&) = delete;
  PreinstalledAppsBrowserTest& operator=(const PreinstalledAppsBrowserTest&) =
      delete;
  ~PreinstalledAppsBrowserTest() override = default;

  // Note: This is different than SetUpCommandLine();
  // SetUpDefaultCommandLine() is called second (surprisingly), so removing
  // the disable pre-installed apps switch in SetUpCommandLine is insufficient.
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpDefaultCommandLine(command_line);
    // We rely on pre-installed apps being present for these tests.
    command_line->RemoveSwitch(::switches::kDisablePreinstalledApps);

    if (ShouldEnableWebAppMigration()) {
      feature_list_.InitAndEnableFeature(
          web_app::kMigrateDefaultChromeAppToWebAppsNonGSuite);
    } else {
      feature_list_.InitAndDisableFeature(
          web_app::kMigrateDefaultChromeAppToWebAppsNonGSuite);
    }
  }

  // Waits for the extension system to be ready, including installing any
  // pending extensions.
  virtual void WaitForSystemReady() {
    {
      base::RunLoop run_loop;
      ExtensionSystem::Get(profile())->ready().Post(FROM_HERE,
                                                    run_loop.QuitClosure());
      run_loop.Run();
    }

    PendingExtensionManager* const pending_manager =
        ExtensionSystem::Get(profile())
            ->extension_service()
            ->pending_extension_manager();

    // If the test extension is still pending, wait for it to finish.
    if (pending_manager->IsIdPending(kDefaultInstalledId)) {
      TestExtensionRegistryObserver(registry()).WaitForExtensionInstalled();
    }

    // In Chromium builds, there shouldn't be any other pending extensions.
    // In Google Chrome, we don't have this assertion, because we bundle a
    // couple other default extensions (like the Chrome Apps In-Apps Payment
    // app, or Chrome Media Router). These will never install, since they rely
    // on being downloaded (which can't happen in browser tests).
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    EXPECT_FALSE(pending_manager->HasPendingExtensions());
#endif
  }

  virtual bool ShouldEnableWebAppMigration() { return false; }

  ExtensionRegistry* registry() { return ExtensionRegistry::Get(profile()); }

 private:
  base::ScopedPathOverride preinstalled_apps_;
  base::test::ScopedFeatureList feature_list_;
};

class PreinstalledAppsMigrationBrowserTest
    : public PreinstalledAppsBrowserTest {
 public:
  PreinstalledAppsMigrationBrowserTest()
      : test_web_app_provider_creator_(base::BindRepeating(
            &PreinstalledAppsMigrationBrowserTest::CreateTestWebAppProvider,
            base::Unretained(this))) {
    // Skip migration on startup because we override the configs used, and need
    // to do that a little later (because we need the embedded test server up
    // and running).
    web_app::PreinstalledWebAppManager::SkipStartupForTesting();
    SetPreinstalledAppIdForTesting(kDefaultInstalledId);
  }
  PreinstalledAppsMigrationBrowserTest(
      const PreinstalledAppsMigrationBrowserTest&) = delete;
  PreinstalledAppsMigrationBrowserTest& operator=(
      const PreinstalledAppsMigrationBrowserTest&) = delete;
  ~PreinstalledAppsMigrationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PreinstalledAppsBrowserTest::SetUpOnMainThread();
    web_app::PreinstalledWebAppManager::
        BypassOfflineManifestRequirementForTesting();
    ASSERT_TRUE(embedded_test_server()->Start());

    app_configs_.push_back(*base::JSONReader::Read(
        base::StringPrintf(kTestAppConfig, GetAppUrl().spec().c_str())));
    web_app::PreinstalledWebAppManager::SetConfigsForTesting(&app_configs_);
  }

  void TearDownOnMainThread() override {
    web_app::PreinstalledWebAppManager::SetConfigsForTesting(nullptr);
    PreinstalledAppsBrowserTest::TearDownOnMainThread();
  }

  bool ShouldEnableWebAppMigration() override {
    bool enable_feature = false;
    // Simulate the switch going back and forth between states.
    // Step 1 (pre=4): Disabled (extension app is installed).
    // Step 2 (pre=3): Enabled (extension app is uninstalled).
    // Step 3 (pre=2): Disabled, simulating a rollback (extension app is
    //                 re-installed).
    // Step 4 (pre=1): Enabled (extension app is re-uninstalled).
    // Step 5 (pre=0): Enabled (extension app stay re-uninstalled).
    size_t pre_count = GetTestPreCount();
    if (pre_count == 4 || pre_count == 2) {
      enable_feature = false;
    } else if (pre_count == 3 || pre_count <= 1) {
      enable_feature = true;
    } else {
      NOTREACHED();
    }

    return enable_feature;
  }

  // We override this to also wait for the PreinstalledWebAppManager.
  void WaitForSystemReady() override {
    PreinstalledAppsBrowserTest::WaitForSystemReady();

    // For web app migration tests, we want to set up extension app shortcut
    // locations to test that they are preserved.
    if (ShouldEnableWebAppMigration()) {
      web_app::ShortcutLocations locations;
      locations.on_desktop = true;
      locations.in_startup = true;
      shortcut_manager_->SetAppExistingShortcuts(GURL("http://example.com/"),
                                                 locations);
    }

    {
      web_app::PreinstalledWebAppManager& web_app_manager =
          web_app::WebAppProvider::Get(profile())
              ->preinstalled_web_app_manager();
      base::RunLoop run_loop;
      auto quit =
          [this, quit_closure = run_loop.QuitClosure()](
              std::map<GURL,
                       web_app::ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, bool> uninstall_results) {
            install_results_ = std::move(install_results);
            std::move(quit_closure).Run();
          };
      web_app_manager.LoadAndSynchronizeForTesting(
          base::BindLambdaForTesting(quit));
      run_loop.Run();
    }
  }

  // Returns a test URL to use for web app installation.
  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  // Returns true if the pre-installed app was migrated to a web app, according
  // to prefs.
  bool WasMigratedToWebApp() {
    return web_app::WasAppMigratedToWebApp(profile(), kDefaultInstalledId);
  }

  // Returns true if the web app was installed, according to the result
  // from the PreinstalledWebAppManager.
  bool WasWebAppInstalledInThisRun() {
    auto iter = install_results_.find(GetAppUrl());
    return iter != install_results_.end() &&
           iter->second.code == web_app::InstallResultCode::kSuccessNewInstall;
  }

  // Returns true if the web app is currently installed in this profile (even if
  // it was installed from a previous run).
  bool IsWebAppCurrentlyInstalled() {
    const web_app::AppId app_id =
        web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GetAppUrl());
    return web_app::WebAppProvider::Get(profile())->registrar().IsInstalled(
        app_id);
  }

  bool CanWebAppAlwaysUpdateIdentity() {
    const web_app::AppId app_id =
        web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, GetAppUrl());
    const web_app::WebApp* web_app = web_app::WebAppProvider::Get(profile())
                                         ->registrar()
                                         .AsWebAppRegistrar()
                                         ->GetAppById(app_id);
    return CanWebAppUpdateIdentity(web_app);
  }

  ExtensionRegistry* registry() { return ExtensionRegistry::Get(profile()); }

 protected:
  web_app::TestShortcutManager* shortcut_manager_;
  web_app::TestOsIntegrationManager* os_integration_manager_;

 private:
  std::unique_ptr<KeyedService> CreateTestWebAppProvider(Profile* profile) {
    auto provider = std::make_unique<web_app::TestWebAppProvider>(profile);
    auto shortcut_manager =
        std::make_unique<web_app::TestShortcutManager>(profile);
    shortcut_manager_ = shortcut_manager.get();
    auto os_integration_manager =
        std::make_unique<web_app::TestOsIntegrationManager>(
            profile, std::move(shortcut_manager), nullptr, nullptr, nullptr);
    os_integration_manager_ = os_integration_manager.get();
    provider->SetOsIntegrationManager(std::move(os_integration_manager));
    provider->Start();
    return provider;
  }

  web_app::TestWebAppProviderCreator test_web_app_provider_creator_;
  std::vector<base::Value> app_configs_;
  std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>
      install_results_;
};

class PreinstalledAppsMigrationEnabledBrowserTest
    : public PreinstalledAppsMigrationBrowserTest {
 public:
  bool ShouldEnableWebAppMigration() override { return true; }
};

class PreinstalledAppsMigrationEnabledThenRolledBackBrowserTest
    : public PreinstalledAppsMigrationBrowserTest {
 public:
  bool ShouldEnableWebAppMigration() override {
    // Simulate the switch going back and forth between states.
    // Step 0 (pre=1): Enabled (web app is installed).
    // Step 1 (pre=0): Disabled (extension app is re-installed).
    switch (GetTestPreCount()) {
      case 1:
        return true;
      case 0:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }
};

// Default apps are handled differently on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Install pre-installed apps, then uninstall one. It should not return on next
// run.
IN_PROC_BROWSER_TEST_F(PreinstalledAppsBrowserTest, PRE_TestUninstall) {
  WaitForSystemReady();
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));

  UninstallExtension(kDefaultInstalledId);
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}
IN_PROC_BROWSER_TEST_F(PreinstalledAppsBrowserTest, TestUninstall) {
  WaitForSystemReady();
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

// A fun back-and-forth with enabling-and-disabling the web app migration
// feature. This is designed to exercise the flow needed in case of a rollback.
IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       PRE_PRE_PRE_PRE_TestRollbackCompatibility) {
  // Initially, the migration feature is disabled, and the extension app should
  // be installed.
  WaitForSystemReady();
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
  EXPECT_FALSE(WasMigratedToWebApp());
  EXPECT_FALSE(WasWebAppInstalledInThisRun());
  EXPECT_FALSE(IsWebAppCurrentlyInstalled());
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       PRE_PRE_PRE_TestRollbackCompatibility) {
  // Next, the feature is enabled. The web app should be installed, and the
  // extension app uninstalled.
  TestExtensionRegistryObserver observer(registry(), kDefaultInstalledId);
  base::RunLoop extension_shortcuts_deleted_loop;
  web_app::WaitForExtensionShortcutsDeleted(
      kDefaultInstalledId, extension_shortcuts_deleted_loop.QuitClosure());
  WaitForSystemReady();
  EXPECT_TRUE(WasMigratedToWebApp());
  EXPECT_TRUE(WasWebAppInstalledInThisRun());
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());

  // Subtle: The uninstallation happens extra-asynchronously (even after it's
  // reported as happening through the PreinstalledWebAppManager).
  ASSERT_TRUE(observer.WaitForExtensionUninstalled());
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));

  // Verify that the migration preserves shortcut states of the uninstalled
  // extension app. The shortcuts for the new app are not created until after
  // the old shortcuts have been deleted, so wait for that first.
  extension_shortcuts_deleted_loop.Run();
  EXPECT_EQ(1u, os_integration_manager_->num_create_shortcuts_calls());
  EXPECT_TRUE(os_integration_manager_->did_add_to_desktop());
  auto options = os_integration_manager_->get_last_install_options();
  EXPECT_TRUE(options->os_hooks[web_app::OsHookType::kRunOnOsLogin]);
  EXPECT_FALSE(options->add_to_quick_launch_bar);
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       PRE_PRE_TestRollbackCompatibility) {
  // Now, the feature is disabled again (simulating an experiment rollback).
  // The extension app should be re-installed.
  WaitForSystemReady();
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
  EXPECT_FALSE(WasMigratedToWebApp());
  EXPECT_FALSE(WasWebAppInstalledInThisRun());
  EXPECT_FALSE(IsWebAppCurrentlyInstalled());
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       PRE_TestRollbackCompatibility) {
  // Finally, re-enable the feature (simulating us fixing the glitch).
  // The extension app should be re-uninstalled.
  TestExtensionRegistryObserver observer(registry(), kDefaultInstalledId);
  base::RunLoop extension_shortcuts_deleted_loop;
  web_app::WaitForExtensionShortcutsDeleted(
      kDefaultInstalledId, extension_shortcuts_deleted_loop.QuitClosure());
  WaitForSystemReady();
  EXPECT_TRUE(WasMigratedToWebApp());
  EXPECT_TRUE(WasWebAppInstalledInThisRun());
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());

  // Subtle: The uninstallation happens extra-asynchronously (even after it's
  // reported as happening through the PreinstalledWebAppManager).
  ASSERT_TRUE(observer.WaitForExtensionUninstalled());
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));

  // Verify that the migration preserves shortcut states of the uninstalled
  // extension app. The shortcuts for the new app are not created until after
  // the old shortcuts have been deleted, so wait for that first.
  extension_shortcuts_deleted_loop.Run();
  EXPECT_EQ(1u, os_integration_manager_->num_create_shortcuts_calls());
  EXPECT_TRUE(os_integration_manager_->did_add_to_desktop());
  auto options = os_integration_manager_->get_last_install_options();
  EXPECT_TRUE(options->os_hooks[web_app::OsHookType::kRunOnOsLogin]);
  EXPECT_FALSE(options->add_to_quick_launch_bar);
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       TestRollbackCompatibility) {
  // Web app should stay installed and extension stay uninstalled on second
  // launch.
  TestExtensionRegistryObserver observer(registry(), kDefaultInstalledId);
  WaitForSystemReady();
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());

  // Verify that there's no redundant shortcut calls.
  // Verify that the migration preserves shortcut states of the uninstalled
  // extension app.
  EXPECT_EQ(0u, os_integration_manager_->num_create_shortcuts_calls());

  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       PRE_PRE_TestExtensionWasAlreadyUninstalled) {
  // To start, the feature is disabled. Wait for the extension to be added,
  // and then uninstall it.
  WaitForSystemReady();
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
  EXPECT_FALSE(WasMigratedToWebApp());
  EXPECT_FALSE(WasWebAppInstalledInThisRun());
  EXPECT_FALSE(IsWebAppCurrentlyInstalled());

  UninstallExtension(kDefaultInstalledId);
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       PRE_TestExtensionWasAlreadyUninstalled) {
  // Now, the feature is enabled. But since the extension was uninstalled, it
  // should not be migrated (or marked as migrated in prefs).
  WaitForSystemReady();
  EXPECT_FALSE(WasMigratedToWebApp());

  EXPECT_FALSE(WasWebAppInstalledInThisRun());
  EXPECT_FALSE(IsWebAppCurrentlyInstalled());
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       TestExtensionWasAlreadyUninstalled) {
  // Web app should stay uninstalled on second launch.
  WaitForSystemReady();
  EXPECT_FALSE(IsWebAppCurrentlyInstalled());

  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationEnabledBrowserTest,
                       PRE_TestAppInstalled) {
  // Migration feature enabled on first launch. Web app should be installed
  WaitForSystemReady();
  EXPECT_TRUE(WasMigratedToWebApp());
  EXPECT_TRUE(WasWebAppInstalledInThisRun());
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());

  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationEnabledBrowserTest,
                       TestAppInstalled) {
  // Web app should stay installed on second launch.
  WaitForSystemReady();
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());

  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

IN_PROC_BROWSER_TEST_F(
    PreinstalledAppsMigrationEnabledThenRolledBackBrowserTest,
    PRE_TestAppInstalled) {
  // Migration feature enabled on first launch. Web app should be installed
  WaitForSystemReady();
  EXPECT_TRUE(WasMigratedToWebApp());
  EXPECT_TRUE(WasWebAppInstalledInThisRun());
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());

  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}

IN_PROC_BROWSER_TEST_F(
    PreinstalledAppsMigrationEnabledThenRolledBackBrowserTest,
    TestAppInstalled) {
  // Extension app should be installed on second launch.
  WaitForSystemReady();
  EXPECT_FALSE(IsWebAppCurrentlyInstalled());

  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kDefaultInstalledId));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(PreinstalledAppsMigrationBrowserTest,
                       TestDefaultAppsCanUpdateIdentity) {
  TestExtensionRegistryObserver observer(registry(), kDefaultInstalledId);
  WaitForSystemReady();
  EXPECT_TRUE(WasMigratedToWebApp());
  EXPECT_TRUE(WasWebAppInstalledInThisRun());
  EXPECT_TRUE(IsWebAppCurrentlyInstalled());
  EXPECT_TRUE(CanWebAppAlwaysUpdateIdentity());
}

}  // namespace extensions

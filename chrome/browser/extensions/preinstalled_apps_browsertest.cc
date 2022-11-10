// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"

#include "base/json/json_reader.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
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
    command_line->RemoveSwitch(::switches::kDisableDefaultApps);

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

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions

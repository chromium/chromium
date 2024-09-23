// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "net/base/filename_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

using extensions::FeatureSwitch;
using extensions::ExtensionRegistry;

// This file contains high-level startup tests for the extensions system. We've
// had many silly bugs where command line flags did not get propagated correctly
// into the services, so we didn't start correctly.

// A waiter for manifest content script loads. The waiter finishes when all of
// its observed extensions have finished loading their manifest scripts.
class ManifestContentScriptWaiter
    : public extensions::UserScriptLoader::Observer {
 public:
  ManifestContentScriptWaiter() = default;
  ~ManifestContentScriptWaiter() = default;
  ManifestContentScriptWaiter(const ManifestContentScriptWaiter& other) =
      delete;
  ManifestContentScriptWaiter& operator=(
      const ManifestContentScriptWaiter& other) = delete;

  // Adds an extension for this waiter to wait on their next script load.
  void Observe(extensions::UserScriptLoader* loader) {
    scoped_observation_.AddObservation(loader);
  }

  // Start waiting for manifest scripts to be loaded.
  void Wait() {
    if (scoped_observation_.IsObservingAnySource())
      run_loop_.Run();
  }

 private:
  // UserScriptLoader::Observer:
  void OnScriptsLoaded(extensions::UserScriptLoader* loader,
                       content::BrowserContext* browser_context) override {
    ASSERT_TRUE(loader->initial_load_complete());
    scoped_observation_.RemoveObservation(loader);
    if (!scoped_observation_.IsObservingAnySource())
      run_loop_.Quit();
  }

  void OnUserScriptLoaderDestroyed(
      extensions::UserScriptLoader* loader) override {
    scoped_observation_.RemoveObservation(loader);
  }

  base::RunLoop run_loop_;

  base::ScopedMultiSourceObservation<extensions::UserScriptLoader,
                                     extensions::UserScriptLoader::Observer>
      scoped_observation_{this};
};

class ExtensionStartupTestBase : public InProcessBrowserTest {
 public:
  ExtensionStartupTestBase() : unauthenticated_load_allowed_(true) {
    num_expected_extensions_ = 3;
  }

 protected:
  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (!load_extensions_.empty()) {
      base::FilePath::StringType paths = base::JoinString(
          load_extensions_, base::FilePath::StringType(1, ','));
      command_line->AppendSwitchNative(extensions::switches::kLoadExtension,
                                       paths);
      command_line->AppendSwitch(
          extensions::switches::kDisableExtensionsFileAccessCheck);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    } else {
      // In Windows and MacOS builds, it is not possible to disable settings
      // enforcement.
      unauthenticated_load_allowed_ = false;
#endif
    }
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath profile_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
    profile_dir = profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir);
    base::CreateDirectory(profile_dir);

    preferences_file_ = profile_dir.Append(chrome::kPreferencesFilename);
    user_scripts_dir_ = profile_dir.AppendASCII("User Scripts");
    extensions_dir_ = profile_dir.AppendASCII("Extensions");

    if (load_extensions_.empty()) {
      base::FilePath src_dir;
      base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good");

      base::CopyFile(src_dir.Append(chrome::kPreferencesFilename),
                     preferences_file_);
      base::CopyDirectory(src_dir.AppendASCII("Extensions"), profile_dir,
                          true);  // recursive
    }
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Bots are on a domain, turn off the domain check for settings hardening in
    // order to be able to test all SettingsEnforcement groups.
    chrome_prefs::DisableDomainCheckForTesting();
  }

  void TearDown() override {
    EXPECT_TRUE(base::DeleteFile(preferences_file_));

    // TODO(phajdan.jr): Check return values of the functions below, carefully.
    base::DeletePathRecursively(user_scripts_dir_);
    base::DeletePathRecursively(extensions_dir_);

    InProcessBrowserTest::TearDown();
  }

  static int GetNonComponentEnabledExtensionCount(Profile* profile) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    int found_extensions = 0;
    for (const auto& extension : registry->enabled_extensions()) {
      if (!extensions::Manifest::IsComponentLocation(extension->location()))
        ++found_extensions;
    }
    return found_extensions;
  }

  void WaitForServicesToStart(int num_expected_extensions,
                              bool expect_extensions_enabled) {
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(browser()->profile());
    // Wait until the extension system is ready.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    if (!unauthenticated_load_allowed_)
      num_expected_extensions = 0;
    ASSERT_EQ(num_expected_extensions,
              GetNonComponentEnabledExtensionCount(browser()->profile()));

    ASSERT_EQ(expect_extensions_enabled,
              extension_system->extension_service()->extensions_enabled());

    if (num_expected_extensions == 0)
      return;

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());

    ManifestContentScriptWaiter waiter;
    extensions::UserScriptManager* manager =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->user_script_manager();

    for (const auto& extension : registry->enabled_extensions()) {
      extensions::ExtensionUserScriptLoader* loader =
          manager->GetUserScriptLoaderForExtension(extension->id());

      // Do not wait for extensions which have no manifest scripts or have
      // already finished a script load.
      if (!extensions::ContentScriptsInfo::GetContentScripts(extension.get())
               .empty() &&
          !loader->initial_load_complete()) {
        waiter.Observe(loader);
      }
    }

    waiter.Wait();
  }

  void TestInjection(bool expect_css, bool expect_script) {
    if (!unauthenticated_load_allowed_) {
      expect_css = false;
      expect_script = false;
    }

    // Load a page affected by the content script and test to see the effect.
    base::FilePath test_file;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file);
    test_file =
        test_file.AppendASCII("extensions").AppendASCII("test_file.html");

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), net::FilePathToFileURL(test_file)));

    EXPECT_EQ(
        expect_css,
        content::EvalJs(
            browser()->tab_strip_model()->GetActiveWebContents(),
            "document.defaultView.getComputedStyle(document.body, null)."
            "getPropertyValue('background-color') == 'rgb(245, 245, 220)'"));

    EXPECT_EQ(
        expect_script,
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "document.title == 'Modified'"));
  }

  base::FilePath preferences_file_;
  base::FilePath extensions_dir_;
  base::FilePath user_scripts_dir_;
  // True unless unauthenticated extension settings are not allowed to be
  // loaded in this configuration.
  bool unauthenticated_load_allowed_;
  // Extensions to load from the command line.
  std::vector<base::FilePath::StringType> load_extensions_;

  int num_expected_extensions_;

  // TODO(https://crbug.com/40804030): Remove when these tests use only MV3
  // extensions.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

// ExtensionsStartupTest
// Ensures that we can startup the browser with --enable-extensions and some
// extensions installed and see them run and do basic things.
using ExtensionStartupTest = ExtensionStartupTestBase;

IN_PROC_BROWSER_TEST_F(ExtensionStartupTest, Test) {
  WaitForServicesToStart(num_expected_extensions_, true);
  TestInjection(true, true);
}

// Tests that disallowing file access on an extension prevents it from injecting
// script into a page with a file URL.
IN_PROC_BROWSER_TEST_F(ExtensionStartupTest, NoFileAccess) {
  WaitForServicesToStart(num_expected_extensions_, true);

  // Keep a separate list of extensions for which to disable file access, since
  // doing so reloads them.
  std::vector<const extensions::Extension*> extension_list;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  for (extensions::ExtensionSet::const_iterator it =
           registry->enabled_extensions().begin();
       it != registry->enabled_extensions().end(); ++it) {
    if ((*it)->location() == extensions::mojom::ManifestLocation::kComponent)
      continue;
    if (extensions::util::AllowFileAccess((*it)->id(), browser()->profile()))
      extension_list.push_back(it->get());
  }

  extensions::UserScriptManager* manager =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->user_script_manager();

  for (size_t i = 0; i < extension_list.size(); ++i) {
    extensions::ExtensionId id = extension_list[i]->id();
    extensions::TestExtensionRegistryObserver registry_observer(registry, id);
    ManifestContentScriptWaiter waiter;

    extensions::util::SetAllowFileAccess(id, browser()->profile(), false);
    registry_observer.WaitForExtensionLoaded();
    extensions::ExtensionUserScriptLoader* loader =
        manager->GetUserScriptLoaderForExtension(id);
    if (!loader->initial_load_complete()) {
      waiter.Observe(loader);
      waiter.Wait();
    }
  }

  TestInjection(false, false);
}

// ExtensionsLoadTest
// Ensures that we can startup the browser with --load-extension and see them
// run.
class ExtensionsLoadTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadTest() {
    base::FilePath one_extension_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &one_extension_path);
    one_extension_path = one_extension_path.AppendASCII("extensions")
                             .AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                             .AppendASCII("1.0.0.0");
    load_extensions_.push_back(one_extension_path.value());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsLoadTest, Test) {
  WaitForServicesToStart(1, true);
  TestInjection(true, true);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(ExtensionsLoadTest,
                       SigninProfileCommandLineExtensionsDontLoad) {
  // The --load-extension command line flag should not be applied to the sign-in
  // profile.
  EXPECT_EQ(0, GetNonComponentEnabledExtensionCount(
                   ash::ProfileHelper::GetSigninProfile()));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// ExtensionsLoadMultipleTest
// Ensures that we can startup the browser with multiple extensions
// via --load-extension=X1,X2,X3.
class ExtensionsLoadMultipleTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadMultipleTest() {
    base::FilePath one_extension_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &one_extension_path);
    one_extension_path = one_extension_path.AppendASCII("extensions")
                             .AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                             .AppendASCII("1.0.0.0");
    load_extensions_.push_back(one_extension_path.value());

    base::FilePath second_extension_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &second_extension_path);
    second_extension_path =
        second_extension_path.AppendASCII("extensions").AppendASCII("app");
    load_extensions_.push_back(second_extension_path.value());

    base::FilePath third_extension_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &third_extension_path);
    third_extension_path =
        third_extension_path.AppendASCII("extensions").AppendASCII("app1");
    load_extensions_.push_back(third_extension_path.value());

    base::FilePath fourth_extension_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &fourth_extension_path);
    fourth_extension_path =
        fourth_extension_path.AppendASCII("extensions").AppendASCII("app2");
    load_extensions_.push_back(fourth_extension_path.value());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsLoadMultipleTest, Test) {
  WaitForServicesToStart(4, true);
  TestInjection(true, true);
}

// TODO(catmullings): Remove test in future chrome release, perhaps M59.
class DeprecatedLoadComponentExtensionSwitchBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  DeprecatedLoadComponentExtensionSwitchBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override;

  ExtensionRegistry* GetExtensionRegistry() {
    return ExtensionRegistry::Get(browser()->profile());
  }
};

void DeprecatedLoadComponentExtensionSwitchBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  base::FilePath fp1(test_data_dir_.AppendASCII("app_dot_com_app/"));
  base::FilePath fp2(test_data_dir_.AppendASCII("app/"));

  command_line->AppendSwitchASCII(
      "load-component-extension",
      fp1.AsUTF8Unsafe() + "," + fp2.AsUTF8Unsafe());
}

// Tests that the --load-component-extension flag is not supported.
IN_PROC_BROWSER_TEST_F(DeprecatedLoadComponentExtensionSwitchBrowserTest,
                       DefunctLoadComponentExtensionFlag) {
  EXPECT_TRUE(extension_service()->extensions_enabled());

  // Checks that the extensions loaded with the --load-component-extension flag
  // are not installed.
  bool is_app_dot_com_extension_installed = false;
  bool is_app_test_extension_installed = false;
  for (const scoped_refptr<const extensions::Extension>& extension :
       GetExtensionRegistry()->enabled_extensions()) {
    if (extension->name() == "App Dot Com: The App") {
      is_app_dot_com_extension_installed = true;
    } else if (extension->name() == "App Test") {
      is_app_test_extension_installed = true;
    } else {
      EXPECT_TRUE(
          extensions::Manifest::IsComponentLocation(extension->location()));
    }
  }
  EXPECT_FALSE(is_app_dot_com_extension_installed);
  EXPECT_FALSE(is_app_test_extension_installed);
}

class DisableExtensionsExceptBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  DisableExtensionsExceptBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override;

  ExtensionRegistry* GetExtensionRegistry() {
    return ExtensionRegistry::Get(browser()->profile());
  }
};

void DisableExtensionsExceptBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  base::FilePath fp1(test_data_dir_.AppendASCII("app_dot_com_app/"));
  base::FilePath fp2(test_data_dir_.AppendASCII("app/"));

  command_line->AppendSwitchASCII(
      switches::kDisableExtensionsExcept,
      fp1.AsUTF8Unsafe() + "," + fp2.AsUTF8Unsafe());

  command_line->AppendSwitch(switches::kNoErrorDialogs);
}

// Tests disabling all extensions except those listed
// (--disable-extensions-except).
IN_PROC_BROWSER_TEST_F(DisableExtensionsExceptBrowserTest,
                       DisableExtensionsExceptFlag) {
  EXPECT_FALSE(extension_service()->extensions_enabled());

  // Checks that the extensions loaded with the --disable-extensions-except flag
  // are enabled.
  bool is_app_dot_com_extension_enabled = false;
  bool is_app_test_extension_enabled = false;
  for (const scoped_refptr<const extensions::Extension>& extension :
       GetExtensionRegistry()->enabled_extensions()) {
    if (extension->name() == "App Dot Com: The App") {
      is_app_dot_com_extension_enabled = true;
    } else if (extension->name() == "App Test") {
      is_app_test_extension_enabled = true;
    } else {
      EXPECT_TRUE(
          extensions::Manifest::IsComponentLocation(extension->location()));
    }
  }
  EXPECT_TRUE(is_app_dot_com_extension_enabled);
  EXPECT_TRUE(is_app_test_extension_enabled);
}

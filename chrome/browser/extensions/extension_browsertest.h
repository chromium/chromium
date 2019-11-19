// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest.h"

class Profile;
struct WebApplicationInfo;

namespace extensions {
class ExtensionCacheFake;
class ExtensionService;
class ExtensionSet;
class ProcessManager;

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest : virtual public InProcessBrowserTest {
 public:
  // Different types of extension's lazy background contexts used in some tests.
  enum class ContextType {
    // A non-persistent background page/JS based extension.
    kEventPage,
    // A Service Worker based extension.
    kServiceWorker,
    // An extension with a persistent background page.
    kPersistentBackground,
  };

 protected:
  // Flags used to configure how the tests are run.
  enum Flags {
    kFlagNone = 0,

    // Allow the extension to run in incognito mode.
    kFlagEnableIncognito = 1 << 0,

    // Allow file access for the extension.
    kFlagEnableFileAccess = 1 << 1,

    // Don't fail when the loaded manifest has warnings (should only be used
    // when testing deprecated features).
    kFlagIgnoreManifestWarnings = 1 << 2,

    // Allow older manifest versions (typically these can't be loaded - we allow
    // them for testing).
    kFlagAllowOldManifestVersions = 1 << 3,

    // Pass the FOR_LOGIN_SCREEN flag when loading the extension. This flag is
    // usually provided for force-installed extension on the login screen.
    kFlagLoadForLoginScreen = 1 << 4,

    // Load the provided extension as Service Worker based extension.
    kFlagRunAsServiceWorkerBasedExtension = 1 << 5,
  };

  ExtensionBrowserTest();
  ~ExtensionBrowserTest() override;

  // Useful accessors.
  ExtensionService* extension_service() {
    return ExtensionSystem::Get(profile())->extension_service();
  }

  ExtensionRegistry* extension_registry() {
    return ExtensionRegistry::Get(profile());
  }

  const std::string& last_loaded_extension_id() {
    return observer_->last_loaded_extension_id();
  }

  // Get the profile to use.
  virtual Profile* profile();

  // Extensions used in tests are typically not from the web store and will have
  // missing content verification hashes. The default implementation disables
  // content verification; this should be overridden by derived tests which care
  // about content verification.
  virtual bool ShouldEnableContentVerification();

  // Extensions used in tests are typically not from the web store and will fail
  // install verification. The default implementation disables install
  // verification; this should be overridden by derived tests which care
  // about install verification.
  virtual bool ShouldEnableInstallVerification();

  // Returns the path of the directory from which to serve resources when they
  // are prefixed with "_test_resources/".
  // The default is chrome/test/data/extensions/.
  virtual base::FilePath GetTestResourcesParentDir();

  static const Extension* GetExtensionByPath(const ExtensionSet& extensions,
                                             const base::FilePath& path);

  // InProcessBrowserTest
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  const Extension* LoadExtension(const base::FilePath& path);

  // Load extension and enable it in incognito mode.
  const Extension* LoadExtensionIncognito(const base::FilePath& path);

  // Load extension from the |path| folder. |flags| is bit mask of values from
  // |Flags| enum.
  const Extension* LoadExtensionWithFlags(const base::FilePath& path,
                                          int flags);

  // Same as above, but sets the installation parameter to the extension
  // preferences.
  const Extension* LoadExtensionWithInstallParam(
      const base::FilePath& path,
      int flags,
      const std::string& install_param);

  // Converts an extension from |path| to a Service Worker based extension and
  // returns true on success.
  // If successful, |out_path| contains path of the converted extension.
  //
  // NOTE: The conversion works only for extensions with background.scripts and
  // background.persistent = false; persistent background pages and
  // background.page are not supported.
  bool CreateServiceWorkerBasedExtension(const base::FilePath& path,
                                         base::FilePath* out_path);

  // Loads unpacked extension from |path| with manifest |manifest_relative_path|
  // and imitates that it is a component extension.
  // |manifest_relative_path| is relative to |path|.
  const Extension* LoadExtensionAsComponentWithManifest(
      const base::FilePath& path,
      const base::FilePath::CharType* manifest_relative_path);

  // Loads unpacked extension from |path| and imitates that it is a component
  // extension. Equivalent to
  // LoadExtensionAsComponentWithManifest(path, kManifestFilename).
  const Extension* LoadExtensionAsComponent(const base::FilePath& path);

  // Loads and launches the app from |path|, and returns it.
  const Extension* LoadAndLaunchApp(const base::FilePath& path);

  // Launches |extension| as a window and returns the browser.
  Browser* LaunchAppBrowser(const Extension* extension);
  // Launches |extension| as a tab and returns the browser.
  Browser* LaunchBrowserForAppInTab(const Extension* extension);

  // Pack the extension in |dir_path| into a crx file and return its path.
  // Return an empty FilePath if there were errors.
  base::FilePath PackExtension(
      const base::FilePath& dir_path,
      int extra_run_flags = ExtensionCreator::kNoRunFlags);

  // Pack the extension in |dir_path| into a crx file at |crx_path|, using the
  // key |pem_path|. If |pem_path| does not exist, create a new key at
  // |pem_out_path|.
  // Return the path to the crx file, or an empty FilePath if there were errors.
  base::FilePath PackExtensionWithOptions(
      const base::FilePath& dir_path,
      const base::FilePath& crx_path,
      const base::FilePath& pem_path,
      const base::FilePath& pem_out_path,
      int extra_run_flags = ExtensionCreator::kNoRunFlags);

  // |expected_change| indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  const Extension* InstallExtension(const base::FilePath& path,
                                    int expected_change) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_NONE, expected_change);
  }

  // Same as above, but an install source other than Manifest::INTERNAL can be
  // specified.
  const Extension* InstallExtension(const base::FilePath& path,
                                    int expected_change,
                                    Manifest::Location install_source) {
    return InstallOrUpdateExtension(std::string(),
                                    path,
                                    INSTALL_UI_TYPE_NONE,
                                    expected_change,
                                    install_source);
  }

  // Installs an extension and grants it the permissions it requests.
  // TODO(devlin): It seems like this is probably the desired outcome most of
  // the time - otherwise the extension installs in a disabled state.
  const Extension* InstallExtensionWithPermissionsGranted(
      const base::FilePath& file_path,
      int expected_change) {
    return InstallOrUpdateExtension(
        std::string(), file_path, INSTALL_UI_TYPE_NONE, expected_change,
        Manifest::INTERNAL, browser(), Extension::NO_FLAGS, false, true);
  }

  // Installs bookmark app for |info|.
  const Extension* InstallBookmarkApp(WebApplicationInfo info);

  // Installs extension as if it came from the Chrome Webstore.
  const Extension* InstallExtensionFromWebstore(const base::FilePath& path,
                                                int expected_change);

  // Same as above but passes an id to CrxInstaller and does not allow a
  // privilege increase.
  const Extension* UpdateExtension(const std::string& id,
                                   const base::FilePath& path,
                                   int expected_change) {
    return InstallOrUpdateExtension(id, path, INSTALL_UI_TYPE_NONE,
                                    expected_change);
  }

  // Same as UpdateExtension but waits for the extension to be idle first.
  const Extension* UpdateExtensionWaitForIdle(const std::string& id,
                                              const base::FilePath& path,
                                              int expected_change);

  // Same as |InstallExtension| but with the normal extension UI showing up
  // (for e.g. info bar on success).
  const Extension* InstallExtensionWithUI(const base::FilePath& path,
                                          int expected_change) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_NORMAL, expected_change);
  }

  const Extension* InstallExtensionWithUIAutoConfirm(const base::FilePath& path,
                                                     int expected_change,
                                                     Browser* browser) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_AUTO_CONFIRM, expected_change,
        browser, Extension::NO_FLAGS);
  }

  const Extension* InstallExtensionWithSourceAndFlags(
      const base::FilePath& path,
      int expected_change,
      Manifest::Location install_source,
      Extension::InitFromValueFlags creation_flags) {
    return InstallOrUpdateExtension(std::string(), path, INSTALL_UI_TYPE_NONE,
                                    expected_change, install_source, browser(),
                                    creation_flags, false, false);
  }

  // Begins install process but simulates a user cancel.
  const Extension* StartInstallButCancel(const base::FilePath& path) {
    return InstallOrUpdateExtension(
        std::string(), path, INSTALL_UI_TYPE_CANCEL, 0);
  }

  void ReloadExtension(const std::string& extension_id);

  void UnloadExtension(const std::string& extension_id);

  void UninstallExtension(const std::string& extension_id);

  void DisableExtension(const std::string& extension_id);

  void EnableExtension(const std::string& extension_id);

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count) {
    return observer_->WaitForPageActionVisibilityChangeTo(count);
  }

  // Wait for an extension install error to be raised. Returns true if an
  // error was raised.
  bool WaitForExtensionInstallError() {
    return observer_->WaitForExtensionInstallError();
  }

  // Waits for an extension load error. Returns true if the error really
  // happened.
  bool WaitForExtensionLoadError() {
    return observer_->WaitForExtensionLoadError();
  }

  // Wait for the specified extension to crash. Returns true if it really
  // crashed.
  bool WaitForExtensionCrash(const std::string& extension_id) {
    return observer_->WaitForExtensionCrash(extension_id);
  }

  // Wait for the crx installer to be done. Returns true if it has finished
  // successfully.
  bool WaitForCrxInstallerDone() {
    return observer_->WaitForCrxInstallerDone();
  }

  // Wait for all extension views to load.
  bool WaitForExtensionViewsToLoad() {
    return observer_->WaitForExtensionViewsToLoad();
  }

  // Wait for the extension to be idle.
  bool WaitForExtensionIdle(const std::string& extension_id) {
    return observer_->WaitForExtensionIdle(extension_id);
  }

  // Wait for the extension to not be idle.
  bool WaitForExtensionNotIdle(const std::string& extension_id) {
    return observer_->WaitForExtensionNotIdle(extension_id);
  }

  // Simulates a page calling window.open on an URL and waits for the
  // navigation.
  // |should_succeed| indicates whether the navigation should succeed, in which
  // case the last committed url should match the passed url and the page should
  // not be an error or interstitial page.
  void OpenWindow(content::WebContents* contents,
                  const GURL& url,
                  bool newtab_process_should_equal_opener,
                  bool should_succeed,
                  content::WebContents** newtab_result);

  // Simulates a page navigating itself to an URL and waits for the
  // navigation.
  void NavigateInRenderer(content::WebContents* contents, const GURL& url);

  // Looks for an ExtensionHost whose URL has the given path component
  // (including leading slash).  Also verifies that the expected number of hosts
  // are loaded.
  ExtensionHost* FindHostWithPath(ProcessManager* manager,
                                  const std::string& path,
                                  int expected_hosts);

  // Returns
  // browsertest_util::ExecuteScriptInBackgroundPage(profile(),
  // extension_id, script).
  std::string ExecuteScriptInBackgroundPage(
      const std::string& extension_id,
      const std::string& script,
      extensions::browsertest_util::ScriptUserActivation
          script_user_activation =
              extensions::browsertest_util::ScriptUserActivation::kActivate);

  // Returns
  // browsertest_util::ExecuteScriptInBackgroundPageNoWait(
  // profile(), extension_id, script).
  bool ExecuteScriptInBackgroundPageNoWait(const std::string& extension_id,
                                           const std::string& script);

  bool loaded_;
  bool installed_;

#if defined(OS_CHROMEOS)
  // True if the command line should be tweaked as if ChromeOS user is
  // already logged in.
  bool set_chromeos_user_;
#endif

  // Set to "chrome/test/data/extensions". Derived classes may override.
  // TODO(michaelpg): Don't override protected data members.
  base::FilePath test_data_dir_;

  std::unique_ptr<ChromeExtensionTestNotificationObserver> observer_;

 private:
  // Temporary directory for testing.
  base::ScopedTempDir temp_dir_;

  // Specifies the type of UI (if any) to show during installation and what
  // user action to simulate.
  enum InstallUIType {
    INSTALL_UI_TYPE_NONE,
    INSTALL_UI_TYPE_CANCEL,
    INSTALL_UI_TYPE_NORMAL,
    INSTALL_UI_TYPE_AUTO_CONFIRM,
  };

  const Extension* InstallOrUpdateExtension(const std::string& id,
                                            const base::FilePath& path,
                                            InstallUIType ui_type,
                                            int expected_change);
  const Extension* InstallOrUpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      int expected_change,
      Browser* browser,
      Extension::InitFromValueFlags creation_flags);
  const Extension* InstallOrUpdateExtension(const std::string& id,
                                            const base::FilePath& path,
                                            InstallUIType ui_type,
                                            int expected_change,
                                            Manifest::Location install_source);
  const Extension* InstallOrUpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      int expected_change,
      Manifest::Location install_source,
      Browser* browser,
      Extension::InitFromValueFlags creation_flags,
      bool wait_for_idle,
      bool grant_permissions);

  // Make the current channel "dev" for the duration of the test.
  ScopedCurrentChannel current_channel_;

  // Disable external install UI.
  FeatureSwitch::ScopedOverride override_prompt_for_external_extensions_;

#if defined(OS_WIN)
  // Use mock shortcut directories to ensure app shortcuts are cleaned up.
  base::ScopedPathOverride user_desktop_override_;
  base::ScopedPathOverride common_desktop_override_;
  base::ScopedPathOverride user_quick_launch_override_;
  base::ScopedPathOverride start_menu_override_;
  base::ScopedPathOverride common_start_menu_override_;
#endif

  // The default profile to be used.
  Profile* profile_;

  // Cache cache implementation.
  std::unique_ptr<ExtensionCacheFake> test_extension_cache_;

  // An override so that chrome-extensions://<extension_id>/_test_resources/foo
  // maps to chrome/test/data/extensions/foo.
  ExtensionProtocolTestHandler test_protocol_handler_;

  // Conditionally disable content verification.
  std::unique_ptr<ScopedIgnoreContentVerifierForTest>
      ignore_content_verification_;

  // Conditionally disable install verification.
  std::unique_ptr<ScopedInstallVerifierBypassForTest>
      ignore_install_verification_;

  // Used to disable CRX publisher signature checking.
  SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;

  ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_check_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBrowserTest);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

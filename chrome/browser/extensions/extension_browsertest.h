// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/browser/extensions/extension_browsertest_platform_delegate.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/test/base/platform_browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#endif

class Profile;

namespace content {
class RenderFrameHost;
class ServiceWorkerContext;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionCache;
class ExtensionHost;
class ExtensionRegistrar;
class ExtensionSet;
class ExtensionTestNotificationObserver;
class ProcessManager;
class ScopedIgnoreContentVerifierForTest;

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionService;
#endif

// A cross-platform base class for extensions-related browser tests.
// `PlatformBrowserTest` inherits from different test suites based on the
// platform; `ExtensionBrowserTest` provides additional functionality
// that is available on all platforms.
class ExtensionBrowserTest : public PlatformBrowserTest,
                             public ExtensionRegistryObserver {
 public:
  using LoadOptions = extensions::browser_test_util::LoadOptions;
  using ContextType = extensions::browser_test_util::ContextType;

  explicit ExtensionBrowserTest(ContextType context_type = ContextType::kNone);
  ExtensionBrowserTest(const ExtensionBrowserTest&) = delete;
  ExtensionBrowserTest& operator=(const ExtensionBrowserTest&) = delete;
  ~ExtensionBrowserTest() override;

 protected:
  // Specifies the type of UI (if any) to show during installation and what
  // user action to simulate.
  enum class InstallUIType {
    kNone,
    kCancel,
    kNormal,
    kAutoConfirm,
  };

  // The platform delegate is an implementation detail of the test harness
  // and should be able to access anything any general test would access.
  friend class ExtensionBrowserTestPlatformDelegate;

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

  // Whether MV2 extensions should be allowed. Defaults to true for testing
  // (since many tests are parameterized to exercise both MV2 + MV3 logic).
  virtual bool ShouldAllowMV2Extensions();

  // Returns the extension in `extensions` with the given `path`, if one exists.
  static const Extension* GetExtensionByPath(const ExtensionSet& extensions,
                                             const base::FilePath& path);

  // content::BrowserTestBase:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDown() override;
  void TearDownOnMainThread() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // Lower-case to match ExtensionBrowserTest.
  ExtensionRegistry* extension_registry();
  ExtensionRegistrar* extension_registrar();

  // Returns the path of the directory from which to serve resources when they
  // are prefixed with "_test_resources/".
  // The default is chrome/test/data/extensions/.
  virtual base::FilePath GetTestResourcesParentDir();

  const Extension* LoadExtension(const base::FilePath& path);
  const Extension* LoadExtension(const base::FilePath& path,
                                 const LoadOptions& options);

  // Loads unpacked extension from `path` with manifest `manifest_relative_path`
  // and imitates that it is a component extension.
  // `manifest_relative_path` is relative to `path`.
  const Extension* LoadExtensionAsComponentWithManifest(
      const base::FilePath& path,
      const base::FilePath::CharType* manifest_relative_path);

  // Loads unpacked extension from `path` and imitates that it is a component
  // extension. Equivalent to
  // `LoadExtensionAsComponentWithManifest(path, kManifestFilename)`.
  const Extension* LoadExtensionAsComponent(const base::FilePath& path);

  // `expected_change` indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  const Extension* InstallExtension(const base::FilePath& path,
                                    std::optional<int> expected_change);

  // Same as above, but an install source other than
  // mojom::ManifestLocation::kInternal can be specified.
  const Extension* InstallExtension(const base::FilePath& path,
                                    std::optional<int> expected_change,
                                    mojom::ManifestLocation install_source);

  // Installs an extension and grants it the permissions it requests.
  // TODO(devlin): It seems like this is probably the desired outcome most of
  // the time - otherwise the extension installs in a disabled state.
  const Extension* InstallExtensionWithPermissionsGranted(
      const base::FilePath& file_path,
      std::optional<int> expected_change);

  // Installs extension as if it came from the Chrome Webstore.
  const Extension* InstallExtensionFromWebstore(
      const base::FilePath& path,
      std::optional<int> expected_change);

  // Same as InstallExtensionFromWebstore(), but sets the install as triggered
  // by user download.
  const Extension* InstallExtensionFromWebstoreTriggeredByUserDownload(
      const base::FilePath& path,
      std::optional<int> expected_change);

  const Extension* InstallExtensionWithUIAutoConfirm(
      const base::FilePath& path,
      std::optional<int> expected_change);

  const Extension* InstallExtensionWithSourceAndFlags(
      const base::FilePath& path,
      std::optional<int> expected_change,
      mojom::ManifestLocation install_source,
      Extension::InitFromValueFlags creation_flags);

  // Begins install process but simulates a user cancel.
  const Extension* StartInstallButCancel(const base::FilePath& path);

  // Same as above but passes an id to CrxInstaller and does not allow a
  // privilege increase.
  const Extension* UpdateExtension(const extensions::ExtensionId& id,
                                   const base::FilePath& path,
                                   std::optional<int> expected_change);

  // Same as UpdateExtension but waits for the extension to be idle first.
  const Extension* UpdateExtensionWaitForIdle(
      const extensions::ExtensionId& id,
      const base::FilePath& path,
      std::optional<int> expected_change);

  void DisableExtension(const ExtensionId& extension_id);
  void DisableExtension(const ExtensionId& extension_id,
                        const DisableReasonSet& disable_reasons);

  // Unloads the extension with the given `extension_id`.
  void UnloadExtension(const ExtensionId& extension_id);

  // Uninstalls the extension with the given `extension_id`.
  void UninstallExtension(const ExtensionId& extension_id);

  // Enables the extension with the given `extension_id`.
  void EnableExtension(const ExtensionId& extension_id);

  // Reloads the extension with the given `extension_id`.
  void ReloadExtension(const ExtensionId& extension_id);

  // Returns the WebContents of the currently active tab.
  // Note that when the test first launches, this will be the same as the
  // default tab's web_contents(). However, if the test creates new tabs and
  // switches the active tab, this will return the WebContents of the new active
  // tab.
  content::WebContents* GetActiveWebContents() const;

  // Returns incognito profile. Creates the profile if it doesn't exist.
  Profile* GetOrCreateIncognitoProfile();

  // Pack the extension in `dir_path` into a crx file and return its path.
  // Return an empty FilePath if there were errors.
  base::FilePath PackExtension(
      const base::FilePath& dir_path,
      int extra_run_flags = ExtensionCreator::kNoRunFlags);

  // Pack the extension in `dir_path` into a crx file at `crx_path`, using the
  // key `pem_path`. If `pem_path` does not exist, create a new key at
  // `pem_out_path`.
  // Return the path to the crx file, or an empty FilePath if there were errors.
  base::FilePath PackExtensionWithOptions(
      const base::FilePath& dir_path,
      const base::FilePath& crx_path,
      const base::FilePath& pem_path,
      const base::FilePath& pem_out_path,
      int extra_run_flags = ExtensionCreator::kNoRunFlags);

  // Navigates to a `url` in the active web contents and waits until the
  // navigation finishes. Returns true on success.
  [[nodiscard]] bool NavigateToURL(const GURL& url);

  // Puts the current tab title in |title|. Returns true on success.
  bool GetCurrentTabTitle(std::u16string* title);

  // Opens `url` in an incognito browser window with the incognito profile of
  // `profile`, blocking until the navigation finishes. Returns the WebContents
  // for `url`.
  content::WebContents* PlatformOpenURLOffTheRecord(Profile* profile,
                                                    const GURL& url);

  // Opens `url` in a new tab, blocking until the navigation finishes.
  content::RenderFrameHost* NavigateToURLInNewTab(const GURL& url);

  // Simulates a page calling window.open on an URL and waits for the
  // navigation.
  // `should_succeed` indicates whether the navigation should succeed, in which
  // case the last committed url should match the passed url and the page should
  // not be an error or interstitial page.
  void OpenWindow(content::WebContents* contents,
                  const GURL& url,
                  bool newtab_process_should_equal_opener,
                  bool should_succeed,
                  content::WebContents** newtab_result);

  // Simulates a page navigating itself to an URL and waits for the
  // navigation. Returns true if the navigation succeeds.
  [[nodiscard]] bool NavigateInRenderer(content::WebContents* contents,
                                        const GURL& url);

  // Looks for an ExtensionHost whose URL has the given path component
  // (including leading slash).  Also verifies that the expected number of hosts
  // are loaded.
  ExtensionHost* FindHostWithPath(ProcessManager* manager,
                                  const std::string& path,
                                  int expected_hosts);

  // Get the ServiceWorkerContext for the default browser's profile.
  content::ServiceWorkerContext* GetServiceWorkerContext();

  // Get the ServiceWorkerContext for the `browser_context`.
  static content::ServiceWorkerContext* GetServiceWorkerContext(
      content::BrowserContext* browser_context);

  // Returns the number of tabs in the current window.
  int GetTabCount();

  // Returns whether the tab at `index` is selected.
  bool IsTabSelected(int index);

  // Closes the tab associated with `web_contents`.
  void CloseTabForWebContents(content::WebContents* web_contents);

  // Waits until `script` calls "chrome.test.sendScriptResult(result)",
  // where `result` is a serializable value, and returns `result`. Fails
  // the test and returns an empty base::Value if `extension_id` isn't
  // installed in the test's profile or doesn't have a background page, or
  // if executing the script fails. The argument `script_user_activation`
  // determines if the script should be executed after a user activation.
  base::Value ExecuteScriptInBackgroundPage(
      const extensions::ExtensionId& extension_id,
      const std::string& script,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);

  // Waits until `script` calls "window.domAutomationController.send(result)",
  // where `result` is a string, and returns `result`. Fails the test and
  // returns an empty base::Value if `extension_id` isn't installed in test's
  // profile or doesn't have a background page, or if executing the script
  // fails. The argument `script_user_activation` determines if the script
  // should be executed after a user activation.
  std::string ExecuteScriptInBackgroundPageDeprecated(
      const extensions::ExtensionId& extension_id,
      const std::string& script,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);

  bool ExecuteScriptInBackgroundPageNoWait(
      const extensions::ExtensionId& extension_id,
      const std::string& script,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);

  // Sets up `test_protocol_handler_` so that
  // chrome-extensions://<extension_id>/_test_resources/foo maps to
  // chrome/test/data/extensions/foo.
  void SetUpTestProtocolHandler();

  // Tears down test protocol handler.
  void TearDownTestProtocolHandler();

  // Wait for all extension views to load.
  bool WaitForExtensionViewsToLoad();

  // Wait for the extension to be idle.
  bool WaitForExtensionIdle(const ExtensionId& extension_id);

  // Wait for the extension to not be idle.
  bool WaitForExtensionNotIdle(const ExtensionId& extension_id);

  // These match the methods in ExtensionBrowserTestPlatformDelegate:
  const Extension* LoadAndLaunchApp(const base::FilePath& path,
                                    bool uses_guest_view = false);
  bool WaitForPageActionVisibilityChangeTo(int count);

  // Lower case to match the style of InProcessBrowserTest.
  virtual Profile* profile();

  // WebContents* of the default tab or nullptr if the default tab is destroyed.
  content::WebContents* web_contents();

  const ExtensionId& last_loaded_extension_id() {
    return last_loaded_extension_id_;
  }
  void set_last_loaded_extension_id(ExtensionId id) {
    last_loaded_extension_id_ = std::move(id);
  }

  ExtensionTestNotificationObserver* test_notification_observer() {
    return test_notification_observer_.get();
  }

  ExtensionBrowserTestPlatformDelegate& platform_delegate() {
    return platform_delegate_;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: ExtensionService is not available in desktop android builds.
  ExtensionService* extension_service();
#endif

  // Set to "chrome/test/data/extensions". Derived classes may override.
  base::FilePath test_data_dir_;

  const ContextType context_type_;

  // An override so that chrome-extensions://<extension_id>/_test_resources/foo
  // maps to chrome/test/data/extensions/foo.
  ExtensionProtocolTestHandler test_protocol_handler_;

#if BUILDFLAG(IS_CHROMEOS)
  // True if the command line should be tweaked as if ChromeOS user is
  // already logged in.
  bool set_chromeos_user_ = true;
#endif

 private:
  // Common implementation for all our various install and update methods.
  const Extension* InstallOrUpdateExtension(
      const extensions::ExtensionId& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      std::optional<int> expected_change,
      mojom::ManifestLocation install_source,
      content::WebContents* active_web_contents,
      Extension::InitFromValueFlags creation_flags,
      bool wait_for_idle,
      bool grant_permissions,
      bool was_triggered_by_user_download);

  ExtensionBrowserTestPlatformDelegate platform_delegate_;

  // Temporary directory for testing.
  base::ScopedTempDir temp_dir_;

  // WebContents* of the default tab or nullptr if the default tab is destroyed.
  base::WeakPtr<content::WebContents> web_contents_;

  ExtensionId last_loaded_extension_id_;

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  class TestTabModel;
  std::unique_ptr<TestTabModel> tab_model_;
#endif

  // Used for setting the default scoped current channel for extension browser
  // tests to UNKNOWN (trunk), in order to enable channel restricted features.
  // TODO(crbug.com/40261741): We should remove this and have the current
  // channel respect what is defined on the builder. If a test requires a
  // specific channel for a channel restricted feature, it should be defining
  // its own scoped channel override. As this stands, it means we don't really
  // have non-trunk coverage for most extension browser tests.
  ScopedCurrentChannel current_channel_;

  // Disable external install UI.
  FeatureSwitch::ScopedOverride override_prompt_for_external_extensions_;

#if BUILDFLAG(IS_WIN)
  // Use mock shortcut directories to ensure app shortcuts are cleaned up.
  base::ScopedPathOverride user_desktop_override_;
  base::ScopedPathOverride common_desktop_override_;
  base::ScopedPathOverride user_quick_launch_override_;
  base::ScopedPathOverride start_menu_override_;
  base::ScopedPathOverride common_start_menu_override_;
#endif

  std::unique_ptr<ExtensionCache> test_extension_cache_;

  // Conditionally disable install verification.
  std::unique_ptr<ScopedInstallVerifierBypassForTest>
      ignore_install_verification_;

  // Conditionally disable content verification.
  std::unique_ptr<ScopedIgnoreContentVerifierForTest>
      ignore_content_verification_;

  // Used to disable CRX publisher signature checking.
  SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;

  ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_check_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Allows MV2 extensions to be loaded.
  std::optional<ScopedTestMV2Enabler> mv2_enabler_;
#endif

  std::unique_ptr<ExtensionTestNotificationObserver>
      test_notification_observer_;

  // Listens to extension loaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

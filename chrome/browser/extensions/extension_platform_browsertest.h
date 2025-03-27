// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/test/base/platform_browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature_channel.h"

class Profile;

namespace content {
class RenderFrameHost;
class ServiceWorkerContext;
class WebContents;
}

namespace extensions {
class Extension;
class ExtensionHost;
class ExtensionBrowserTestPlatformDelegate;
class ExtensionRegistrar;
class ProcessManager;

// A cross-platform base class for extensions-related browser tests.
// `PlatformBrowserTest` inherits from different test suites based on the
// platform; `ExtensionPlatformBrowserTest` provides additional functionality
// that is available on all platforms.
class ExtensionPlatformBrowserTest : public PlatformBrowserTest,
                                     public ExtensionRegistryObserver {
 public:
  using LoadOptions = extensions::browser_test_util::LoadOptions;
  using ContextType = extensions::browser_test_util::ContextType;

  explicit ExtensionPlatformBrowserTest(
      ContextType context_type = ContextType::kNone);
  ExtensionPlatformBrowserTest(const ExtensionPlatformBrowserTest&) = delete;
  ExtensionPlatformBrowserTest& operator=(const ExtensionPlatformBrowserTest&) =
      delete;
  ~ExtensionPlatformBrowserTest() override;

 protected:
  // The platform delegate is an implementation detail of the test harness
  // and should be able to access anything any general test would access.
  friend class ExtensionBrowserTestPlatformDelegate;

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

  void DisableExtension(const ExtensionId& extension_id);
  void DisableExtension(const ExtensionId& extension_id,
                        const DisableReasonSet& disable_reasons);

  // Unloads the extension with the given `extension_id`.
  void UnloadExtension(const ExtensionId& extension_id);

  // Uninstalls the extension with the given `extension_id`.
  void UninstallExtension(const ExtensionId& extension_id);

  // Enables the extension with the given `extension_id`.
  void EnableExtension(const ExtensionId& extension_id);

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

  // Pack the extension in `dir_path` into a crx file at |crx_path|, using the
  // key `pem_path`. If `pem_path` does not exist, create a new key at
  // `pem_out_path`.
  // Return the path to the crx file, or an empty FilePath if there were errors.
  base::FilePath PackExtensionWithOptions(
      const base::FilePath& dir_path,
      const base::FilePath& crx_path,
      const base::FilePath& pem_path,
      const base::FilePath& pem_out_path,
      int extra_run_flags = ExtensionCreator::kNoRunFlags);

  // Opens `url` in an incognito browser window with the incognito profile of
  // `profile`, blocking until the navigation finishes. Returns the WebContents
  // for `url`.
  content::WebContents* PlatformOpenURLOffTheRecord(Profile* profile,
                                                    const GURL& url);

  // Opens `url` in a new tab, blocking until the navigation finishes.
  content::RenderFrameHost* NavigateToURLInNewTab(const GURL& url);

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

  // Waits until |script| calls "window.domAutomationController.send(result)",
  // where |result| is a string, and returns |result|. Fails the test and
  // returns an empty base::Value if |extension_id| isn't installed in test's
  // profile or doesn't have a background page, or if executing the script
  // fails. The argument |script_user_activation| determines if the script
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

  // Set to "chrome/test/data/extensions". Derived classes may override.
  base::FilePath test_data_dir_;

  const ContextType context_type_;

  // An override so that chrome-extensions://<extension_id>/_test_resources/foo
  // maps to chrome/test/data/extensions/foo.
  ExtensionProtocolTestHandler test_protocol_handler_;

 private:
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

  // Listens to extension loaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_

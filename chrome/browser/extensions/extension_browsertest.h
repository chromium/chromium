// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/browser/extensions/extension_platform_browsertest.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

class Profile;

namespace extensions {
class ChromeExtensionTestNotificationObserver;
class ExtensionBrowserTestPlatformDelegate;
class ExtensionCacheFake;
class ExtensionService;
class ExtensionSet;
class WindowController;

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest : public ExtensionPlatformBrowserTest {
 public:
  using ContextType = extensions::browser_test_util::ContextType;
  using LoadOptions = extensions::browser_test_util::LoadOptions;

  ExtensionBrowserTest(const ExtensionBrowserTest&) = delete;
  ExtensionBrowserTest& operator=(const ExtensionBrowserTest&) = delete;

  bool IsContextTypeForServiceWorker() const {
    return IsServiceWorkerContext(context_type_);
  }

 protected:
  // The platform delegate is an implementation detail of the test harness
  // and should be able to access anything any general test would access.
  friend class ExtensionBrowserTestPlatformDelegate;

  explicit ExtensionBrowserTest(ContextType context_type = ContextType::kNone);
  ~ExtensionBrowserTest() override;

  // Useful accessors.
  ExtensionService* extension_service();

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

  static const Extension* GetExtensionByPath(const ExtensionSet& extensions,
                                             const base::FilePath& path);

  // InProcessBrowserTest
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // ExtensionPlatformBrowserTest:
  Profile* profile() final;

  // These functions intentionally shadow the versions in the base class
  // ExtensionPlatformBrowserTest. They cannot be made virtual because there
  // are too many individual tests that define a LoadExtension() function and
  // shadowing virtual functions is not allowed.
  const Extension* LoadExtension(const base::FilePath& path);
  const Extension* LoadExtension(const base::FilePath& path,
                                 const LoadOptions& options);

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

  // Loads and launches the app from |path|, and returns it. Waits until the
  // launched app's WebContents has been created and finished loading. If the
  // app uses a guest view this will create two WebContents (one for the host
  // and one for the guest view). `uses_guest_view` is used to wait for the
  // second WebContents.
  const Extension* LoadAndLaunchApp(const base::FilePath& path,
                                    bool uses_guest_view = false);

  // Launches |extension| as a window and returns the browser.
  Browser* LaunchAppBrowser(const Extension* extension);

  // |expected_change| indicates how many extensions should be installed (or
  // disabled, if negative).
  // 1 means you expect a new install, 0 means you expect an upgrade, -1 means
  // you expect a failed upgrade.
  const Extension* InstallExtension(const base::FilePath& path,
                                    std::optional<int> expected_change) {
    return InstallOrUpdateExtension(std::string(), path, InstallUIType::kNone,
                                    std::move(expected_change));
  }

  // Same as above, but an install source other than
  // mojom::ManifestLocation::kInternal can be specified.
  const Extension* InstallExtension(const base::FilePath& path,
                                    std::optional<int> expected_change,
                                    mojom::ManifestLocation install_source) {
    return InstallOrUpdateExtension(std::string(), path, InstallUIType::kNone,
                                    std::move(expected_change), install_source);
  }

  // Installs an extension and grants it the permissions it requests.
  // TODO(devlin): It seems like this is probably the desired outcome most of
  // the time - otherwise the extension installs in a disabled state.
  const Extension* InstallExtensionWithPermissionsGranted(
      const base::FilePath& file_path,
      std::optional<int> expected_change) {
    return InstallOrUpdateExtension(
        std::string(), file_path, InstallUIType::kNone,
        std::move(expected_change), mojom::ManifestLocation::kInternal,
        GetWindowController(), Extension::NO_FLAGS, false, true);
  }

  // Installs extension as if it came from the Chrome Webstore.
  const Extension* InstallExtensionFromWebstore(
      const base::FilePath& path,
      std::optional<int> expected_change);

  // Same as above but passes an id to CrxInstaller and does not allow a
  // privilege increase.
  const Extension* UpdateExtension(const extensions::ExtensionId& id,
                                   const base::FilePath& path,
                                   std::optional<int> expected_change) {
    return InstallOrUpdateExtension(id, path, InstallUIType::kNone,
                                    std::move(expected_change));
  }

  // Same as UpdateExtension but waits for the extension to be idle first.
  const Extension* UpdateExtensionWaitForIdle(
      const extensions::ExtensionId& id,
      const base::FilePath& path,
      std::optional<int> expected_change);

  const Extension* InstallExtensionWithUIAutoConfirm(
      const base::FilePath& path,
      std::optional<int> expected_change,
      Browser* browser);

  const Extension* InstallExtensionWithSourceAndFlags(
      const base::FilePath& path,
      std::optional<int> expected_change,
      mojom::ManifestLocation install_source,
      Extension::InitFromValueFlags creation_flags) {
    return InstallOrUpdateExtension(
        std::string(), path, InstallUIType::kNone, std::move(expected_change),
        install_source, GetWindowController(), creation_flags, false, false);
  }

  // Begins install process but simulates a user cancel.
  const Extension* StartInstallButCancel(const base::FilePath& path) {
    return InstallOrUpdateExtension(std::string(), path, InstallUIType::kCancel,
                                    0);
  }

  void ReloadExtension(const extensions::ExtensionId& extension_id);

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count);

  // Wait for all extension views to load.
  bool WaitForExtensionViewsToLoad();

  // Wait for the extension to be idle.
  bool WaitForExtensionIdle(const extensions::ExtensionId& extension_id);

  // Wait for the extension to not be idle.
  bool WaitForExtensionNotIdle(const extensions::ExtensionId& extension_id);

#if BUILDFLAG(IS_CHROMEOS)
  // True if the command line should be tweaked as if ChromeOS user is
  // already logged in.
  bool set_chromeos_user_;
#endif

  std::unique_ptr<ChromeExtensionTestNotificationObserver> observer_;

 private:
  // Temporary directory for testing.
  base::ScopedTempDir temp_dir_;

  // Specifies the type of UI (if any) to show during installation and what
  // user action to simulate.
  enum class InstallUIType {
    kNone,
    kCancel,
    kNormal,
    kAutoConfirm,
  };

  const Extension* InstallOrUpdateExtension(const extensions::ExtensionId& id,
                                            const base::FilePath& path,
                                            InstallUIType ui_type,
                                            std::optional<int> expected_change);
  const Extension* InstallOrUpdateExtension(
      const extensions::ExtensionId& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      std::optional<int> expected_change,
      WindowController* window_controller,
      Extension::InitFromValueFlags creation_flags);
  const Extension* InstallOrUpdateExtension(
      const extensions::ExtensionId& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      std::optional<int> expected_change,
      mojom::ManifestLocation install_source);
  const Extension* InstallOrUpdateExtension(
      const extensions::ExtensionId& id,
      const base::FilePath& path,
      InstallUIType ui_type,
      std::optional<int> expected_change,
      mojom::ManifestLocation install_source,
      WindowController* window_controller,
      Extension::InitFromValueFlags creation_flags,
      bool wait_for_idle,
      bool grant_permissions);

  // Returns the WindowController for this test's browser window.
  WindowController* GetWindowController();

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

  // The default profile to be used.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;

  // Cache cache implementation.
  std::unique_ptr<ExtensionCacheFake> test_extension_cache_;

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

  // Allows MV2 extensions to be loaded.
  std::optional<ScopedTestMV2Enabler> mv2_enabler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

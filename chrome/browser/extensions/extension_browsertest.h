// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
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

  // InProcessBrowserTest
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // ExtensionPlatformBrowserTest:
  std::unique_ptr<ExtensionTestNotificationObserver>
  CreateTestNotificationObserver() final;
  Profile* profile() final;

  // Loads and launches the app from |path|, and returns it. Waits until the
  // launched app's WebContents has been created and finished loading. If the
  // app uses a guest view this will create two WebContents (one for the host
  // and one for the guest view). `uses_guest_view` is used to wait for the
  // second WebContents.
  const Extension* LoadAndLaunchApp(const base::FilePath& path,
                                    bool uses_guest_view = false);

  // Wait for the number of visible page actions to change to |count|.
  bool WaitForPageActionVisibilityChangeTo(int count);

#if BUILDFLAG(IS_CHROMEOS)
  // True if the command line should be tweaked as if ChromeOS user is
  // already logged in.
  bool set_chromeos_user_ = true;
#endif

 private:
  // A convenience method to get the ExtensionTestNotificationObserver as its
  // Chrome-side implementation.
  ChromeExtensionTestNotificationObserver*
  GetChromeExtensionTestNotificationObserver();

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
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_ = nullptr;

  // Cache cache implementation.
  std::unique_ptr<ExtensionCacheFake> test_extension_cache_;

  // Conditionally disable install verification.
  std::unique_ptr<ScopedInstallVerifierBypassForTest>
      ignore_install_verification_;

  ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_check_;

  // Allows MV2 extensions to be loaded.
  std::optional<ScopedTestMV2Enabler> mv2_enabler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

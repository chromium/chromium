// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_platform_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sync/model/string_ordinal.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/common/api/web_accessible_resources.h"
#include "extensions/common/api/web_accessible_resources_mv2.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_background_page_waiter.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

using extensions::mojom::ManifestLocation;

namespace extensions {

using extensions::service_worker_test_utils::TestServiceWorkerContextObserver;

ExtensionBrowserTest::ExtensionBrowserTest(ContextType context_type)
    : ExtensionPlatformBrowserTest(context_type),
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(),
          false)
#if BUILDFLAG(IS_WIN)
      ,
      user_desktop_override_(base::DIR_USER_DESKTOP),
      common_desktop_override_(base::DIR_COMMON_DESKTOP),
      user_quick_launch_override_(base::DIR_USER_QUICK_LAUNCH),
      start_menu_override_(base::DIR_START_MENU),
      common_start_menu_override_(base::DIR_COMMON_START_MENU)
#endif
{
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() = default;

ExtensionService* ExtensionBrowserTest::extension_service() {
  return ExtensionSystem::Get(profile())->extension_service();
}

std::unique_ptr<ExtensionTestNotificationObserver>
ExtensionBrowserTest::CreateTestNotificationObserver() {
  return browser() ? std::make_unique<ChromeExtensionTestNotificationObserver>(
                         browser())
                   : std::make_unique<ChromeExtensionTestNotificationObserver>(
                         profile());
}

Profile* ExtensionBrowserTest::profile() {
  if (!profile_) {
    if (browser()) {
      profile_ = browser()->profile();
    } else {
      profile_ = ProfileManager::GetLastUsedProfile();
    }
  }
  return profile_;
}

void ExtensionBrowserTest::SetUp() {
  test_extension_cache_ = std::make_unique<ExtensionCacheFake>();
  ExtensionPlatformBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionPlatformBrowserTest::SetUpCommandLine(command_line);

  if (!ShouldEnableInstallVerification()) {
    ignore_install_verification_ =
        std::make_unique<ScopedInstallVerifierBypassForTest>();
  }

  if (ShouldAllowMV2Extensions()) {
    mv2_enabler_.emplace();
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    "testuser@gmail.com");
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  ExtensionPlatformBrowserTest::SetUpOnMainThread();

  ExtensionUpdater* updater = ExtensionUpdater::Get(profile());
  if (updater->enabled()) {
    updater->SetExtensionCacheForTesting(test_extension_cache_.get());
  }

  content::URLDataSource::Add(profile(),
                              std::make_unique<ThemeSource>(profile()));
}

const Extension* ExtensionBrowserTest::LoadAndLaunchApp(
    const base::FilePath& path,
    bool uses_guest_view) {
  const Extension* app = LoadExtension(path);
  CHECK(app);
  content::CreateAndLoadWebContentsObserver app_loaded_observer(
      /*num_expected_contents=*/uses_guest_view ? 2 : 1);
  apps::AppLaunchParams params(
      app->id(), apps::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(std::move(params));
  app_loaded_observer.Wait();

  return app;
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  return GetChromeExtensionTestNotificationObserver()
      ->WaitForPageActionVisibilityChangeTo(count);
}

ChromeExtensionTestNotificationObserver*
ExtensionBrowserTest::GetChromeExtensionTestNotificationObserver() {
  return static_cast<ChromeExtensionTestNotificationObserver*>(
      test_notification_observer());
}

}  // namespace extensions

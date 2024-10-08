// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {
const char kCanvasAppURL[] = "https://canvas.apps.chrome/";
const char kCanvasAppTitle[] = "canvas.apps.chrome";
const char kWebStoreExtensionURL[] = "https://chrome.google.com/webstore/";
const char kWebStoreExtensionTitle[] = "chrome.google.com";

struct VisibilityFlags {
  bool show_in_search;
  bool show_in_launcher;
  bool show_in_shelf;
};

}  // namespace

class SystemFeaturesPolicyTest : public PolicyTest {
 public:
  SystemFeaturesPolicyTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kEcheSWA, ash::features::kConch},
        /*disabled_features=*/{});
    fake_crostini_features_.set_is_allowed_now(true);
  }

 protected:
  std::u16string GetWebUITitle(const GURL& url,
                               bool using_navigation_throttle) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    if (using_navigation_throttle) {
      content::WaitForLoadStopWithoutSuccessCheck(web_contents);
    } else {
      EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    }
    return web_contents->GetTitle();
  }

  void EnableExtensions(bool skip_session_components) {
    auto* profile = browser()->profile();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->component_loader()
        ->AddDefaultComponentExtensions(skip_session_components);
    base::RunLoop().RunUntilIdle();
  }

  // Disables specified system features or enables all if system_features is
  // empty. Updates disabled mode for disabled system features.
  void UpdateSystemFeaturesDisableList(base::Value system_features,
                                       const char* disabled_mode) {
    PolicyMap policies;
    policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::move(system_features), nullptr);
    if (disabled_mode) {
      policies.Set(key::kSystemFeaturesDisableMode, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                   base::Value(disabled_mode), nullptr);
    }
    // TODO(b/280518509): Remove this workaround once multidevice code
    // supports runtime policy updates.
    policies.Set(key::kPhoneHubAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  // Convenience overload of UpdateSystemFeaturesDisableList() that allows
  // callers to provide a base::Value::List instead of a base::Value.
  void UpdateSystemFeaturesDisableList(base::Value::List system_features,
                                       const char* disabled_mode) {
    UpdateSystemFeaturesDisableList(base::Value(std::move(system_features)),
                                    disabled_mode);
  }

  void VerifyExtensionAppState(const char* app_id,
                               apps::Readiness expected_readiness,
                               bool blocked_icon,
                               const VisibilityFlags& expected_visibility) {
    auto* profile = browser()->profile();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    ASSERT_TRUE(registry->enabled_extensions().GetByID(app_id));
    VerifyAppState(app_id, expected_readiness, blocked_icon,
                   expected_visibility);
  }

  void VerifyAppState(const char* app_id,
                      apps::Readiness expected_readiness,
                      bool blocked_icon,
                      const VisibilityFlags& expected_visibility) {
    auto* profile = browser()->profile();
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

    bool exist = proxy->AppRegistryCache().ForOneApp(
        app_id, [&expected_readiness, &blocked_icon,
                 &expected_visibility](const apps::AppUpdate& update) {
          EXPECT_EQ(expected_readiness, update.Readiness());
          if (blocked_icon) {
            EXPECT_TRUE(apps::IconEffects::kBlocked &
                        update.IconKey()->icon_effects);
          } else {
            EXPECT_FALSE(apps::IconEffects::kBlocked &
                         update.IconKey()->icon_effects);
          }
          EXPECT_EQ(expected_visibility.show_in_launcher,
                    update.ShowInLauncher());
          EXPECT_EQ(expected_visibility.show_in_search, update.ShowInSearch());
          EXPECT_EQ(expected_visibility.show_in_shelf, update.ShowInShelf());
        });
    EXPECT_TRUE(exist);
  }

  void InstallSWAs() {
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }

  void InstallPWA(const GURL& app_url, const char* app_id) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    web_app_info->scope = app_url.GetWithoutFilename();
    webapps::AppId installed_app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    EXPECT_EQ(app_id, installed_app_id);
  }

  VisibilityFlags GetVisibilityFlags(bool is_hidden) {
    VisibilityFlags flags;
    if (is_hidden) {
      flags.show_in_launcher = false;
      flags.show_in_search = false;
      flags.show_in_shelf = false;
      return flags;
    }
    flags.show_in_launcher = true;
    flags.show_in_search = true;
    flags.show_in_shelf = true;
    return flags;
  }

  void VerifyAppDisableMode(const char* app_id, const char* feature) {
    base::Value::List system_features;
    system_features.Append(feature);
    VisibilityFlags expected_visibility =
        GetVisibilityFlags(false /* is_hidden */);
    // Disable app with default mode (blocked).
    UpdateSystemFeaturesDisableList(system_features.Clone(), nullptr);
    VerifyAppState(app_id, apps::Readiness::kDisabledByPolicy, true,
                   expected_visibility);
    // Disable and hide app.
    expected_visibility = GetVisibilityFlags(true /* is_hidden */);
    UpdateSystemFeaturesDisableList(system_features.Clone(),
                                    kHiddenDisableMode);
    VerifyAppState(app_id, apps::Readiness::kDisabledByPolicy, true,
                   expected_visibility);
    // Disable and block app.
    expected_visibility = GetVisibilityFlags(false /* is_hidden */);
    UpdateSystemFeaturesDisableList(system_features.Clone(),
                                    kBlockedDisableMode);
    VerifyAppState(app_id, apps::Readiness::kDisabledByPolicy, true,
                   expected_visibility);
    // Enable app.
    UpdateSystemFeaturesDisableList(base::Value(), nullptr);
    VerifyAppState(app_id, apps::Readiness::kReady, false, expected_visibility);
  }

  // Used for non-link-capturing PWAs.
  void VerifyIsAppURLDisabled(const char* app_id,
                              const char* feature,
                              const char* url,
                              const char* app_title) {
    const GURL& app_url = GURL(url);

    // The URL navigation is still allowed because the app is not installed,
    // though it is disabled by policy.
    base::Value::List system_features;
    system_features.Append(feature);
    UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);
    EXPECT_EQ(base::UTF8ToUTF16(app_title), GetWebUITitle(app_url, true));

    // Install the app.
    InstallPWA(app_url, app_id);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
              GetWebUITitle(app_url, true));

    // Enable the app by removing it from the policy of disabled apps.
    UpdateSystemFeaturesDisableList(base::Value(), nullptr);
    EXPECT_EQ(base::UTF8ToUTF16(app_title), GetWebUITitle(app_url, true));
  }

  void VerifyIsExtensionAppURLAccessible(const char* url,
                                         const char* app_title) {
    const GURL& app_url = GURL(url);
    EXPECT_EQ(base::UTF8ToUTF16(app_title), GetWebUITitle(app_url, true));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  // Fake the Crostini feature to have the Terminal app icon show in the
  // launcher when installed.
  crostini::FakeCrostiniFeatures fake_crostini_features_;
};

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreBeforeInstall) {
  base::Value::List system_features;
  system_features.Append(kWebStoreFeature);
  VisibilityFlags expected_visibility =
      GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);
  EnableExtensions(true);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  // The URL navigation should still be possible
  // even if the app is disabled by policy.
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId, apps::Readiness::kReady,
                          false, expected_visibility);
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreAfterInstall) {
  EnableExtensions(false);
  base::Value::List system_features;
  system_features.Append(kWebStoreFeature);
  VisibilityFlags expected_visibility =
      GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  // The URL navigation should still be possible
  // even if the app is disabled by policy.
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId, apps::Readiness::kReady,
                          false, expected_visibility);
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableWebStoreAfterInstallWithModes) {
  EnableExtensions(false);
  base::Value::List system_features;
  system_features.Append(kWebStoreFeature);
  VisibilityFlags expected_visibility =
      GetVisibilityFlags(false /* is_hidden */);
  // Disable app with default mode (blocked).
  // The URL navigation should still be possible
  // even if the app is disabled by policy.
  UpdateSystemFeaturesDisableList(system_features.Clone(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);
  // Disable and hide app.
  expected_visibility = GetVisibilityFlags(true /* is_hidden */);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);
  // Disable and block app.
  expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kBlockedDisableMode);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);
  // Enable app
  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId, apps::Readiness::kReady,
                          false, expected_visibility);
  VerifyIsExtensionAppURLAccessible(kWebStoreExtensionURL,
                                    kWebStoreExtensionTitle);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableSWAs) {
  InstallSWAs();

  // Disable Camera app.
  VerifyAppDisableMode(web_app::kCameraAppId, kCameraFeature);

  // Disable Explore app.
  VerifyAppDisableMode(web_app::kHelpAppId, kExploreFeature);

  // Disable Gallery app.
  VerifyAppDisableMode(web_app::kMediaAppId, kGalleryFeature);

  // Disable Terminal app.
  VerifyAppDisableMode(guest_os::kTerminalSystemAppId, kTerminalFeature);

  // Disable Print Jobs app.
  VerifyAppDisableMode(web_app::kPrintManagementAppId, kPrintJobsFeature);

  // Disable Key Shortcuts app.
  VerifyAppDisableMode(web_app::kShortcutCustomizationAppId,
                       kKeyShortcutsFeature);

  // Disable Recorder app.
  VerifyAppDisableMode(web_app::kRecorderAppId, kRecorderFeature);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableMultipleAppsWithHiddenModeAfterInstall) {
  InstallSWAs();
  InstallPWA(GURL(kCanvasAppURL), web_app::kCanvasAppId);

  // Disable app with hidden mode.
  const base::Value::List system_features = base::Value::List()
                                                .Append(kCameraFeature)
                                                .Append(kScanningFeature)
                                                .Append(kWebStoreFeature)
                                                .Append(kCanvasFeature)
                                                .Append(kCroshFeature)
                                                .Append(kGalleryFeature)
                                                .Append(kTerminalFeature)
                                                .Append(kPrintJobsFeature)
                                                .Append(kKeyShortcutsFeature)
                                                .Append(kRecorderFeature);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);

  VisibilityFlags expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);
  VerifyAppState(web_app::kCameraAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyAppState(web_app::kScanningAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyAppState(web_app::kCanvasAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyAppState(web_app::kCroshAppId, apps::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyAppState(web_app::kMediaAppId, apps::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyAppState(guest_os::kTerminalSystemAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kPrintManagementAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kShortcutCustomizationAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kRecorderAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);

  // Disable and block apps.
  expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  // Crosh is never shown.
  VisibilityFlags crosh_expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kBlockedDisableMode);

  VerifyAppState(web_app::kCameraAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyAppState(web_app::kScanningAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyAppState(web_app::kCanvasAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyAppState(web_app::kCroshAppId, apps::Readiness::kDisabledByPolicy, true,
                 crosh_expected_visibility);
  VerifyAppState(web_app::kMediaAppId, apps::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyAppState(guest_os::kTerminalSystemAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kPrintManagementAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kShortcutCustomizationAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kRecorderAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);

  // Enable apps.
  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyAppState(web_app::kCameraAppId, apps::Readiness::kReady, false,
                 expected_visibility);
  VerifyAppState(web_app::kScanningAppId, apps::Readiness::kReady, false,
                 expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId, apps::Readiness::kReady,
                          false, expected_visibility);
  VerifyAppState(web_app::kCanvasAppId, apps::Readiness::kReady, false,
                 expected_visibility);
  VerifyAppState(web_app::kCroshAppId, apps::Readiness::kReady, false,
                 crosh_expected_visibility);
  VerifyAppState(web_app::kMediaAppId, apps::Readiness::kReady, false,
                 expected_visibility);
  VerifyAppState(guest_os::kTerminalSystemAppId, apps::Readiness::kReady, false,
                 expected_visibility);
  VerifyAppState(web_app::kPrintManagementAppId, apps::Readiness::kReady, false,
                 expected_visibility);
  VerifyAppState(web_app::kShortcutCustomizationAppId, apps::Readiness::kReady,
                 false, expected_visibility);
  VerifyAppState(web_app::kRecorderAppId, apps::Readiness::kReady, false,
                 expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableMultipleAppsWithHiddenModeBeforeInstall) {
  const base::Value::List system_features = base::Value::List()
                                                .Append(kCameraFeature)
                                                .Append(kScanningFeature)
                                                .Append(kWebStoreFeature)
                                                .Append(kCanvasFeature)
                                                .Append(kCroshFeature)
                                                .Append(kGalleryFeature)
                                                .Append(kTerminalFeature)
                                                .Append(kPrintJobsFeature)
                                                .Append(kKeyShortcutsFeature)
                                                .Append(kRecorderFeature);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);

  InstallSWAs();
  InstallPWA(GURL(kCanvasAppURL), web_app::kCanvasAppId);

  VisibilityFlags expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);

  // Disable app with hidden mode.
  VerifyAppState(web_app::kCameraAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyAppState(web_app::kScanningAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyAppState(web_app::kCanvasAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
  VerifyAppState(web_app::kCroshAppId, apps::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyAppState(web_app::kMediaAppId, apps::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyAppState(guest_os::kTerminalSystemAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kPrintManagementAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kShortcutCustomizationAppId,
                 apps::Readiness::kDisabledByPolicy, true, expected_visibility);
  VerifyAppState(web_app::kRecorderAppId, apps::Readiness::kDisabledByPolicy,
                 true, expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, RedirectChromeSettingsURL) {
  base::Value::List system_features;
  system_features.Append(kBrowserSettingsFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  GURL settings_url = GURL(chrome::kChromeUISettingsURL);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            GetWebUITitle(settings_url, false));

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS),
            GetWebUITitle(settings_url, false));
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, RedirectCroshURL) {
  base::Value::List system_features;
  system_features.Append(kCroshFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  GURL crosh_url = GURL(chrome::kChromeUIUntrustedCroshURL);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            GetWebUITitle(crosh_url, false));

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  // Title is empty for untrusted URLs.
  EXPECT_EQ(std::u16string(), GetWebUITitle(crosh_url, false));
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisablePWAs) {
  // Disable Canvas app.
  VerifyIsAppURLDisabled(web_app::kCanvasAppId, kCanvasFeature, kCanvasAppURL,
                         kCanvasAppTitle);
  VerifyAppDisableMode(web_app::kCanvasAppId, kCanvasFeature);
}

}  // namespace policy

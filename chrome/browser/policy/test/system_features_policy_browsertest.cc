// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {
const char kCanvasAppURL[] = "https://canvas.apps.chrome/";
const char kCanvasAppTitle[] = "canvas.apps.chrome";
const char kGoogleNewsAppURL[] = "https://news.google.com/?lfhs=2";
const char kGoogleNewsAppTitle[] = "news.google.com";

struct VisibilityFlags {
  apps::mojom::OptionalBool show_in_search;
  apps::mojom::OptionalBool show_in_launcher;
  apps::mojom::OptionalBool show_in_shelf;
};

}  // namespace

class SystemFeaturesPolicyTest : public PolicyTest {
 public:
  SystemFeaturesPolicyTest() = default;

 protected:
  std::u16string GetWebUITitle(const GURL& url,
                               bool using_navigation_throttle) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::NavigateToURL(browser(), url);
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
    UpdateProviderPolicy(policies);
  }

  void VerifyExtensionAppState(const char* app_id,
                               apps::mojom::Readiness expected_readiness,
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
                      apps::mojom::Readiness expected_readiness,
                      bool blocked_icon,
                      const VisibilityFlags& expected_visibility) {
    auto* profile = browser()->profile();
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->FlushMojoCallsForTesting();

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
    web_app::WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
  }

  void InstallPWA(const GURL& app_url, const char* app_id) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url.GetWithoutFilename();
    web_app::AppId installed_app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    EXPECT_EQ(app_id, installed_app_id);
    // Wait for app service to see the newly installed app.
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->FlushMojoCallsForTesting();
  }

  VisibilityFlags GetVisibilityFlags(bool is_hidden) {
    VisibilityFlags flags;
    if (is_hidden) {
      flags.show_in_launcher = apps::mojom::OptionalBool::kFalse;
      flags.show_in_search = apps::mojom::OptionalBool::kFalse;
      flags.show_in_shelf = apps::mojom::OptionalBool::kFalse;
      return flags;
    }
    flags.show_in_launcher = apps::mojom::OptionalBool::kTrue;
    flags.show_in_search = apps::mojom::OptionalBool::kTrue;
    flags.show_in_shelf = apps::mojom::OptionalBool::kTrue;
    return flags;
  }

  void VerifyAppDisableMode(const char* app_id, const char* feature) {
    base::Value system_features(base::Value::Type::LIST);
    system_features.Append(feature);
    VisibilityFlags expected_visibility =
        GetVisibilityFlags(false /* is_hidden */);
    // Disable app with default mode (blocked).
    UpdateSystemFeaturesDisableList(system_features.Clone(), nullptr);
    VerifyAppState(app_id, apps::mojom::Readiness::kDisabledByPolicy, true,
                   expected_visibility);
    // Disable and hide app.
    expected_visibility = GetVisibilityFlags(true /* is_hidden */);
    UpdateSystemFeaturesDisableList(system_features.Clone(),
                                    kHiddenDisableMode);
    VerifyAppState(app_id, apps::mojom::Readiness::kDisabledByPolicy, true,
                   expected_visibility);
    // Disable and block app.
    expected_visibility = GetVisibilityFlags(false /* is_hidden */);
    UpdateSystemFeaturesDisableList(system_features.Clone(),
                                    kBlockedDisableMode);
    VerifyAppState(app_id, apps::mojom::Readiness::kDisabledByPolicy, true,
                   expected_visibility);
    // Enable app.
    UpdateSystemFeaturesDisableList(base::Value(), nullptr);
    VerifyAppState(app_id, apps::mojom::Readiness::kReady, false,
                   expected_visibility);
  }

  // Used for non-link-capturing PWAs.
  void VerifyIsAppURLDisabled(const char* app_id,
                              const char* feature,
                              const char* url,
                              const char* app_title) {
    const GURL& app_url = GURL(url);

    // The URL navigation is still allowed because the app is not installed,
    // though it is disabled by policy.
    base::Value system_features(base::Value::Type::LIST);
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
};

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreBeforeInstall) {
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  VisibilityFlags expected_visibility =
      GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);
  EnableExtensions(true);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          expected_visibility);

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kReady, false,
                          expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreAfterInstall) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  VisibilityFlags expected_visibility =
      GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          expected_visibility);

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kReady, false,
                          expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableWebStoreAfterInstallWithModes) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  VisibilityFlags expected_visibility =
      GetVisibilityFlags(false /* is_hidden */);
  // Disable app with default mode (blocked).
  UpdateSystemFeaturesDisableList(system_features.Clone(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  // Disable and hide app.
  expected_visibility = GetVisibilityFlags(true /* is_hidden */);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  // Disable and block app.
  expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kBlockedDisableMode);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  // Enable app
  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kReady, false,
                          expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableSWAs) {
  InstallSWAs();

  // Disable Camera app.
  VerifyAppDisableMode(web_app::kCameraAppId, kCameraFeature);

  // Disable Explore app.
  VerifyAppDisableMode(web_app::kHelpAppId, kExploreFeature);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableMultipleAppsWithHiddenModeAfterInstall) {
  InstallSWAs();
  InstallPWA(GURL(kCanvasAppURL), web_app::kCanvasAppId);

  // Disable app with hidden mode.
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kCameraFeature);
  system_features.Append(kScanningFeature);
  system_features.Append(kWebStoreFeature);
  system_features.Append(kCanvasFeature);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);
  VisibilityFlags camera_expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);
  VisibilityFlags scanning_expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);
  VisibilityFlags web_store_expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);
  VisibilityFlags canvas_expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);
  VerifyAppState(web_app::kCameraAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 camera_expected_visibility);
  VerifyAppState(web_app::kScanningAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 scanning_expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          web_store_expected_visibility);
  VerifyAppState(web_app::kCanvasAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 canvas_expected_visibility);
  // Disable and block apps.
  camera_expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  scanning_expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  // We never show scanning in the launcher.
  scanning_expected_visibility.show_in_launcher =
      apps::mojom::OptionalBool::kFalse;
  web_store_expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  canvas_expected_visibility = GetVisibilityFlags(false /* is_hidden */);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kBlockedDisableMode);
  VerifyAppState(web_app::kCameraAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 camera_expected_visibility);
  VerifyAppState(web_app::kScanningAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 scanning_expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          web_store_expected_visibility);
  VerifyAppState(web_app::kCanvasAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 canvas_expected_visibility);
  // Enable apps.
  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyAppState(web_app::kCameraAppId, apps::mojom::Readiness::kReady, false,
                 camera_expected_visibility);
  VerifyAppState(web_app::kScanningAppId, apps::mojom::Readiness::kReady, false,
                 scanning_expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kReady, false,
                          web_store_expected_visibility);
  VerifyAppState(web_app::kCanvasAppId, apps::mojom::Readiness::kReady, false,
                 canvas_expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableMultipleAppsWithHiddenModeBeforeInstall) {
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kCameraFeature);
  system_features.Append(kScanningFeature);
  system_features.Append(kWebStoreFeature);
  system_features.Append(kCanvasFeature);
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);

  InstallSWAs();
  InstallPWA(GURL(kCanvasAppURL), web_app::kCanvasAppId);

  VisibilityFlags expected_visibility =
      GetVisibilityFlags(true /* is_hidden */);

  // Disable app with hidden mode.
  VerifyAppState(web_app::kCameraAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyAppState(web_app::kScanningAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
  VerifyExtensionAppState(extensions::kWebStoreAppId,
                          apps::mojom::Readiness::kDisabledByPolicy, true,
                          expected_visibility);
  VerifyAppState(web_app::kCanvasAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 expected_visibility);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, RedirectChromeSettingsURL) {
  PolicyMap policies;
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kBrowserSettingsFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  GURL settings_url = GURL(chrome::kChromeUISettingsURL);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            GetWebUITitle(settings_url, false));

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS),
            GetWebUITitle(settings_url, false));
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisablePWAs) {
  // Disable Canvas app.
  VerifyIsAppURLDisabled(web_app::kCanvasAppId, kCanvasFeature, kCanvasAppURL,
                         kCanvasAppTitle);
  VerifyAppDisableMode(web_app::kCanvasAppId, kCanvasFeature);

  // Disable Google News app.
  VerifyIsAppURLDisabled(web_app::kGoogleNewsAppId, kGoogleNewsFeature,
                         kGoogleNewsAppURL, kGoogleNewsAppTitle);
  VerifyAppDisableMode(web_app::kGoogleNewsAppId, kGoogleNewsFeature);
}

}  // namespace policy

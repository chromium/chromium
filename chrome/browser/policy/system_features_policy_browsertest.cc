// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
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

class SystemFeaturesPolicyTest : public PolicyTest {
 public:
  SystemFeaturesPolicyTest() = default;

 protected:
  base::string16 GetWebUITitle(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::NavigateToURL(browser(), url);

    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
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

  void VerifyAppState(const char* app_id,
                      apps::mojom::Readiness expected_readiness,
                      bool blocked_icon,
                      apps::mojom::OptionalBool expected_visibility) {
    auto* profile = browser()->profile();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    ASSERT_TRUE(registry->enabled_extensions().GetByID(app_id));

    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->FlushMojoCallsForTesting();

    proxy->AppRegistryCache().ForOneApp(
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
          EXPECT_EQ(expected_visibility, update.ShowInLauncher());
          EXPECT_EQ(expected_visibility, update.ShowInSearch());
          EXPECT_EQ(expected_visibility, update.ShowInShelf());
        });
  }
};

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreBeforeInstall) {
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);
  EnableExtensions(true);
  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 apps::mojom::OptionalBool::kTrue);

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyAppState(extensions::kWebStoreAppId, apps::mojom::Readiness::kReady,
                 false, apps::mojom::OptionalBool::kTrue);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreAfterInstall) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 apps::mojom::OptionalBool::kTrue);

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyAppState(extensions::kWebStoreAppId, apps::mojom::Readiness::kReady,
                 false, apps::mojom::OptionalBool::kTrue);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableWebStoreAfterInstallWithModes) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  // Disable app with default mode (blocked)..
  UpdateSystemFeaturesDisableList(system_features.Clone(), nullptr);
  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 apps::mojom::OptionalBool::kTrue);
  // Disable and hide app.
  UpdateSystemFeaturesDisableList(system_features.Clone(), kHiddenDisableMode);
  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 apps::mojom::OptionalBool::kFalse);
  // Disable and block app.
  UpdateSystemFeaturesDisableList(system_features.Clone(), kBlockedDisableMode);
  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true,
                 apps::mojom::OptionalBool::kTrue);
  // Enable app
  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  VerifyAppState(extensions::kWebStoreAppId, apps::mojom::Readiness::kReady,
                 false, apps::mojom::OptionalBool::kTrue);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, RedirectChromeSettingsURL) {
  PolicyMap policies;
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kBrowserSettingsFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features), nullptr);

  GURL settings_url = GURL(chrome::kChromeUISettingsURL);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            GetWebUITitle(settings_url));

  UpdateSystemFeaturesDisableList(base::Value(), nullptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS),
            GetWebUITitle(settings_url));
}

}  // namespace policy

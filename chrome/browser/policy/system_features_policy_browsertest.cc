// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class SystemFeaturesPolicyTest : public PolicyTest {
 public:
  SystemFeaturesPolicyTest() {
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kCameraSystemWebApp);
  }

 protected:
  base::string16 GetWebUITitle(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ui_test_utils::NavigateToURL(browser(), url);

    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    return web_contents->GetTitle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableCameraBeforeInstall) {
  PolicyMap policies;
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kCameraFeature);
  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(system_features), nullptr);
  UpdateProviderPolicy(policies);

  auto* profile = browser()->profile();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->component_loader()
      ->AddDefaultComponentExtensions(false);
  base::RunLoop().RunUntilIdle();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->FlushMojoCallsForTesting();

  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kDisabledByPolicy,
                  update.Readiness());
        EXPECT_TRUE(apps::IconEffects::kBlocked &
                    update.IconKey()->icon_effects);
      });

  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(), nullptr);
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kReady, update.Readiness());
        EXPECT_FALSE(apps::IconEffects::kBlocked &
                     update.IconKey()->icon_effects);
      });
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableCameraAfterInstall) {
  auto* profile = browser()->profile();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->component_loader()
      ->AddDefaultComponentExtensions(false);
  base::RunLoop().RunUntilIdle();

  PolicyMap policies;
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kCameraFeature);
  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(system_features), nullptr);
  UpdateProviderPolicy(policies);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kDisabledByPolicy,
                  update.Readiness());
        EXPECT_TRUE(apps::IconEffects::kBlocked &
                    update.IconKey()->icon_effects);
      });

  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(), nullptr);
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));

  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kCameraAppId, [](const apps::AppUpdate& update) {
        EXPECT_EQ(apps::mojom::Readiness::kReady, update.Readiness());
        EXPECT_FALSE(apps::IconEffects::kBlocked &
                     update.IconKey()->icon_effects);
      });
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, RedirectChromeSettingsURL) {
  PolicyMap policies;
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kBrowserSettingsFeature);
  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(system_features), nullptr);
  UpdateProviderPolicy(policies);

  GURL settings_url = GURL(chrome::kChromeUISettingsURL);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            GetWebUITitle(settings_url));

  policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS),
            GetWebUITitle(settings_url));
}

}  // namespace policy

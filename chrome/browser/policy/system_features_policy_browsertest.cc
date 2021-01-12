// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // empty.
  void UpdateSystemFeaturesDisableList(base::Value system_features) {
    PolicyMap policies;
    policies.Set(key::kSystemFeaturesDisableList, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::move(system_features), nullptr);
    UpdateProviderPolicy(policies);
  }

  void VerifyAppState(const char* app_id,
                      apps::mojom::Readiness expected_readiness,
                      bool blocked_icon) {
    auto* profile = browser()->profile();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    ASSERT_TRUE(registry->enabled_extensions().GetByID(app_id));

    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->FlushMojoCallsForTesting();

    proxy->AppRegistryCache().ForOneApp(
        app_id,
        [&expected_readiness, &blocked_icon](const apps::AppUpdate& update) {
          EXPECT_EQ(expected_readiness, update.Readiness());
          if (blocked_icon) {
            EXPECT_TRUE(apps::IconEffects::kBlocked &
                        update.IconKey()->icon_effects);
          } else {
            EXPECT_FALSE(apps::IconEffects::kBlocked &
                         update.IconKey()->icon_effects);
          }
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableCameraBeforeInstall) {
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kCameraFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features));
  EnableExtensions(false);
  VerifyAppState(extension_misc::kCameraAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true);

  UpdateSystemFeaturesDisableList(base::Value());
  VerifyAppState(extension_misc::kCameraAppId, apps::mojom::Readiness::kReady,
                 false);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableCameraAfterInstall) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kCameraFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features));

  VerifyAppState(extension_misc::kCameraAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true);

  UpdateSystemFeaturesDisableList(base::Value());
  VerifyAppState(extension_misc::kCameraAppId, apps::mojom::Readiness::kReady,
                 false);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreBeforeInstall) {
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features));
  EnableExtensions(true);
  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true);

  UpdateSystemFeaturesDisableList(base::Value());
  VerifyAppState(extensions::kWebStoreAppId, apps::mojom::Readiness::kReady,
                 false);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, DisableWebStoreAfterInstall) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features));

  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true);

  UpdateSystemFeaturesDisableList(base::Value());
  VerifyAppState(extensions::kWebStoreAppId, apps::mojom::Readiness::kReady,
                 false);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest,
                       DisableCameraAndWebStoreAfterInstall) {
  EnableExtensions(false);
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kWebStoreFeature);
  system_features.Append(kCameraFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features));

  VerifyAppState(extensions::kWebStoreAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true);
  VerifyAppState(extension_misc::kCameraAppId,
                 apps::mojom::Readiness::kDisabledByPolicy, true);

  UpdateSystemFeaturesDisableList(base::Value());
  VerifyAppState(extensions::kWebStoreAppId, apps::mojom::Readiness::kReady,
                 false);
}

IN_PROC_BROWSER_TEST_F(SystemFeaturesPolicyTest, RedirectChromeSettingsURL) {
  PolicyMap policies;
  base::Value system_features(base::Value::Type::LIST);
  system_features.Append(kBrowserSettingsFeature);
  UpdateSystemFeaturesDisableList(std::move(system_features));

  GURL settings_url = GURL(chrome::kChromeUISettingsURL);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CHROME_URLS_DISABLED_PAGE_HEADER),
            GetWebUITitle(settings_url));

  UpdateSystemFeaturesDisableList(base::Value());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS),
            GetWebUITitle(settings_url));
}

}  // namespace policy

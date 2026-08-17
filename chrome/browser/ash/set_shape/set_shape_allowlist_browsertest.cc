// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace ash {

namespace {

web_app::IsolatedWebAppUrlInfo InstallIsolatedWebAppAndReturnUrlInfo(
    Profile* profile) {
  web_app::IsolatedWebAppUrlInfo url_info =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              .SetName("Set Shape Test IWA")
              .SetDisplayMode(blink::mojom::DisplayMode::kStandalone)
              .SetDisplayModeOverride(
                  {web_app::DisplayOverride::CreateUnframed({})})
              .AddPermissionsPolicy(
                  network::mojom::PermissionsPolicyFeature::kWindowManagement,
                  /*self=*/true, /*origins=*/{}))
          .BuildBundle()
          ->InstallChecked(profile);

  // Grant Window Management permission.
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetContentSettingDefaultScope(
      url_info.origin().GetURL(), url_info.origin().GetURL(),
      ContentSettingsType::WINDOW_MANAGEMENT, CONTENT_SETTING_ALLOW);

  return url_info;
}

}  // namespace

// Verifies the behavior of the setShape API for Isolated Web Apps in ChromeOS
// when the `kSetShape` flag is enabled.
class SetShapeFeatureFlagBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  SetShapeFeatureFlagBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kSetShape, blink::features::kUnframedIwa}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SetShapeFeatureFlagBrowserTest,
                       IsolatedWebAppCanAccessSetShape) {
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());
  content::RenderFrameHost* frame = OpenApp(url_info.app_id());

  EXPECT_EQ(true, content::EvalJs(frame, "'setShape' in window"));
}

IN_PROC_BROWSER_TEST_F(SetShapeFeatureFlagBrowserTest,
                       IsolatedWebAppChildWindowCanAccessSetShape) {
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());

  content::RenderFrameHost* frame = OpenApp(url_info.app_id());

  content::WebContentsAddedObserver child_observer;
  ASSERT_TRUE(content::ExecJs(frame, "window.open('/')"));
  content::WebContents* child_contents = child_observer.GetWebContents();
  content::WaitForLoadStop(child_contents);
  ASSERT_TRUE(child_contents);

  ASSERT_EQ(true, content::EvalJs(child_contents, "'setShape' in window"));
}

IN_PROC_BROWSER_TEST_F(SetShapeFeatureFlagBrowserTest,
                       RegularPageCannotAccessSetShape) {
  std::unique_ptr<net::EmbeddedTestServer> server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps"));
  auto page_url = server->GetOrigin().GetURL().Resolve("basic.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // `window.setShape` is not defined because this is not an IWA.
  EXPECT_EQ(false, content::EvalJs(web_contents, "'setShape' in window"));
}

// Verifies that only IWAs in the allowlist can access the setShape API.
class SetShapeAllowlistBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 private:
  base::test::ScopedFeatureList feature_list_{blink::features::kUnframedIwa};
};

IN_PROC_BROWSER_TEST_F(SetShapeAllowlistBrowserTest, SetShapeIsUndefined) {
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // By default, the IWA is not allowlisted and the flag is disabled.
  EXPECT_EQ(false, content::EvalJs(app_frame, "'setShape' in window"));
}

IN_PROC_BROWSER_TEST_F(SetShapeAllowlistBrowserTest,
                       OnlyAllowlistedIwasCanAccessSetShape) {
  web_app::IsolatedWebAppUrlInfo allowed_url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());
  web_app::IsolatedWebAppUrlInfo non_allowed_url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());

  // Configure the allowlist to allow only the first IWA.
  EXPECT_OK(web_app::test::ConfigureSetShapeAllowlist(
      allowed_url_info.web_bundle_id()));

  content::RenderFrameHost* allowed_frame = OpenApp(allowed_url_info.app_id());
  content::RenderFrameHost* non_allowed_frame =
      OpenApp(non_allowed_url_info.app_id());

  EXPECT_EQ(true, content::EvalJs(allowed_frame, "'setShape' in window"));
  EXPECT_EQ(false, content::EvalJs(non_allowed_frame, "'setShape' in window"));

  // `setShape` returns a resolved `Promise<undefined>` on success.
  auto result = content::EvalJs(allowed_frame, R"(
      window.setShape([
        new DOMRect(0, 0, 200, 200)
      ])
    )");
  EXPECT_EQ(base::Value(), result);
}

// Verifies the behavior when the allowlist is disabled.
class SetShapeAllowlistDisabledBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  SetShapeAllowlistDisabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kUnframedIwa},
        {chromeos::features::kCrosIsolatedWebAppSetShapeAllowlist});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SetShapeAllowlistDisabledBrowserTest,
                       AllowlistDoesNotWork) {
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());

  EXPECT_OK(
      web_app::test::ConfigureSetShapeAllowlist(url_info.web_bundle_id()));

  content::RenderFrameHost* frame = OpenApp(url_info.app_id());

  EXPECT_EQ(false, content::EvalJs(frame, "'setShape' in window"));
}

// Verifies the `kSetShape` flag grants access even if the
// allowlist is disabled.
class SetShapeMainFlagOverridesAllowlistBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  SetShapeMainFlagOverridesAllowlistBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kSetShape, blink::features::kUnframedIwa},
        {chromeos::features::kCrosIsolatedWebAppSetShapeAllowlist});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SetShapeMainFlagOverridesAllowlistBrowserTest,
                       IsolatedWebAppCanAccessSetShape) {
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallIsolatedWebAppAndReturnUrlInfo(profile());

  content::RenderFrameHost* frame = OpenApp(url_info.app_id());

  ASSERT_EQ(true, content::EvalJs(frame, "'setShape' in window"));
}

}  // namespace ash

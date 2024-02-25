// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/components/arc/arc_features_parser.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_webapk_instance.h"
#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/apps/app_service/webapk/webapk_test_server.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace {

constexpr char kWebApkPackageName[] = "org.chromium.webapk.browsertest";

std::optional<arc::ArcFeatures> GetArcFeatures() {
  arc::ArcFeatures arc_features;
  arc_features.build_props.abi_list = "x86";
  return arc_features;
}

}  // namespace

class WebApkPolicyBrowserTest : public policy::PolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    webapk_test_server_ = std::make_unique<apps::WebApkTestServer>();
    webapk_test_server_->SetUpAndStartServer(embedded_test_server());
    webapk_test_server_->RespondWithSuccess(kWebApkPackageName);

    fake_webapk_instance_ = std::make_unique<arc::FakeWebApkInstance>();
    arc::ArcServiceManager::Get()->arc_bridge_service()->webapk()->SetInstance(
        fake_webapk_instance());

    arc_features_getter_ = base::BindRepeating(&GetArcFeatures);
    arc::ArcFeaturesParser::SetArcFeaturesGetterForTesting(
        &arc_features_getter_);
  }

  arc::FakeWebApkInstance* fake_webapk_instance() {
    return fake_webapk_instance_.get();
  }

  bool received_webapk_request() {
    return webapk_test_server_->last_webapk_request() != nullptr;
  }

 private:
  std::unique_ptr<arc::FakeWebApkInstance> fake_webapk_instance_;
  base::RepeatingCallback<std::optional<arc::ArcFeatures>()>
      arc_features_getter_;
  std::unique_ptr<apps::WebApkTestServer> webapk_test_server_;
};

// When there's no policy set, installing a Web App should install a WebAPK.
// This test also acts as an integration test for the WebAPK installation
// process.
IN_PROC_BROWSER_TEST_F(WebApkPolicyBrowserTest, DefaultInstallWebApk) {
  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");

  PrefChangeRegistrar pref_registrar;
  pref_registrar.Init(browser()->profile()->GetPrefs());

  // Wait for the pref to be set, which is the last stage of WebAPK
  // installation.
  base::RunLoop run_loop;
  pref_registrar.Add(apps::webapk_prefs::kGeneratedWebApksPref,
                     run_loop.QuitClosure());

  const webapps::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);
  run_loop.Run();

  ASSERT_TRUE(received_webapk_request());
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(browser()->profile()),
              testing::ElementsAre(app_id));
}

// When the WebAPKs feature disabled by policy, installing a web app should not
// generate a WebAPK.
IN_PROC_BROWSER_TEST_F(WebApkPolicyBrowserTest, DisabledByPolicy) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kArcAppToWebAppSharingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  provider_.UpdateChromePolicy(policies);

  const GURL app_url =
      embedded_test_server()->GetURL("/web_share_target/charts.html");
  const webapps::AppId app_id =
      web_app::InstallWebAppFromManifest(browser(), app_url);

  // Given that we are testing the absence of any WebAPK, we can't wait for
  // anything to show up in Prefs. Instead, run until idle and assume this is
  // enough time for installation to (not) trigger.
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(received_webapk_request());
  ASSERT_THAT(apps::webapk_prefs::GetWebApkAppIds(browser()->profile()),
              testing::IsEmpty());
}

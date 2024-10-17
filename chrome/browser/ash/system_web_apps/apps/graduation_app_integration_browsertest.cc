// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/graduation/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/system_web_apps/apps/graduation_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

constexpr char kContentTransferAppName[] = "Content Transfer";
constexpr char kFromChromeInternalHistogramName[] =
    "Apps.DefaultAppLaunch.FromChromeInternal";

class GraduationAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  GraduationAppIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kGraduation);
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppIntegrationTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

  void SetGraduationEnablement(bool is_enabled) {
    profile()->GetPrefs()->SetDict(
        ash::prefs::kGraduationEnablementStatus,
        base::Value::Dict().Set("is_enabled", is_enabled));
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_P(GraduationAppIntegrationTest, InstallGraduationApp) {
  SetGraduationEnablement(/*is_enabled=*/true);

  const GURL url(ash::graduation::kChromeUIGraduationAppURL);

  // Install all enabled SWAs and validate that the Graduation App has the
  // expected attributes.
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::GRADUATION, url, kContentTransferAppName));
}

IN_PROC_BROWSER_TEST_P(GraduationAppIntegrationTest, RecordMetricsOnLaunch) {
  SetGraduationEnablement(/*is_enabled=*/true);

  histogram_tester().ExpectUniqueSample(kFromChromeInternalHistogramName,
                                        apps::DefaultAppName::kGraduationApp,
                                        0);

  WaitForTestSystemAppInstall();

  const GURL url(ash::graduation::kChromeUIGraduationAppURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::GRADUATION);
  observer.Wait();

  histogram_tester().ExpectUniqueSample(kFromChromeInternalHistogramName,
                                        apps::DefaultAppName::kGraduationApp,
                                        1);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    GraduationAppIntegrationTest);

}  // namespace

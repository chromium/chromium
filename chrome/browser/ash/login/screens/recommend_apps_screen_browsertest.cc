// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"

#include <memory>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/ash/login/screens/recommend_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {
namespace {

constexpr char kRecommendAppsId[] = "recommend-apps";

const test::UIPath kWebviewUIPath = {kRecommendAppsId, "appView"};
const test::UIPath kInstallButton = {kRecommendAppsId, "installButton"};
const test::UIPath kSkipButton = {kRecommendAppsId, "skipButton"};

const test::UIPath kAppsList = {kRecommendAppsId, "appsList"};
const test::UIPath kFirstAppCheckbox = {kRecommendAppsId, "appsList",
                                        R"(test\\.app\\.foo\\.app1)"};
const test::UIPath kSecondAppCheckbox = {kRecommendAppsId, "appsList",
                                         R"(test\\.app\\.foo\\.app2)"};

const std::string kJsonResponse =
    R"json({"recommendedApp": [{
    "androidApp": {
      "packageName": "test.app.foo.app1",
      "title": "Test app 1",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/1IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      }
    }
  }, {
    "androidApp": {
      "packageName": "test.app.foo.app2",
      "title": "Test app 2",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/2IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      }
    }
  }, {
    "androidApp": {
      "packageName": "test.app.foo.app3",
      "title": "Test app 3",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/3IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      }
    }
  }
  ]})json";

bool IsNewRecommendedAppsEnabled() {
  return features::IsOobeNewRecommendAppsEnabled() &&
         base::FeatureList::IsEnabled(::features::kAppDiscoveryForOobe);
}

struct FakeAppInfo {
 public:
  FakeAppInfo(const std::string& package_name, const std::string& name)
      : package_name(package_name), name(name) {}
  ~FakeAppInfo() = default;

  base::Value ToValue() const {
    base::Value result(base::Value::Type::DICTIONARY);
    result.SetKey("package_name", base::Value(package_name));
    result.SetKey("name", base::Value(name));
    return result;
  }

  const std::string package_name;
  const std::string name;
};

class StubRecommendAppsFetcher : public RecommendAppsFetcher {
 public:
  explicit StubRecommendAppsFetcher(RecommendAppsFetcherDelegate* delegate)
      : delegate_(delegate) {}
  ~StubRecommendAppsFetcher() override = default;

  bool started() const { return started_; }

  void SimulateSuccess(const std::vector<FakeAppInfo>& apps) {
    ASSERT_FALSE(IsNewRecommendedAppsEnabled());
    EXPECT_TRUE(started());
    base::Value::List app_list;
    for (const auto& app : apps) {
      app_list.Append(app.ToValue());
    }
    delegate_->OnLoadSuccess(base::Value(std::move(app_list)));
  }

  void SimulateSuccess(bool bad_response = false) {
    ASSERT_TRUE(IsNewRecommendedAppsEnabled());
    EXPECT_TRUE(started());
    if (bad_response) {
      delegate_->OnLoadSuccess(base::Value());
      return;
    }
    auto output = base::JSONReader::ReadAndReturnValueWithError(kJsonResponse);
    ASSERT_TRUE(output.has_value());
    delegate_->OnLoadSuccess(std::move(*output));
  }

  void SimulateParseError() {
    EXPECT_TRUE(started_);
    delegate_->OnParseResponseError();
  }

  void SimulateLoadError() {
    EXPECT_TRUE(started_);
    delegate_->OnLoadError();
  }

  // RecommendAppsFetcher:
  void Start() override {
    EXPECT_FALSE(started_);
    started_ = true;
  }
  void Retry() override { NOTREACHED(); }

 protected:
  RecommendAppsFetcherDelegate* const delegate_;
  bool started_ = false;
};

// Param represents if both kAppDiscoveryForOobe and kOobeNewRecommendApps are
// enabled.
class RecommendAppsScreenTest : public OobeBaseTest,
                                public testing::WithParamInterface<bool> {
 public:
  RecommendAppsScreenTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {::features::kAppDiscoveryForOobe, features::kOobeNewRecommendApps},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {},
          {::features::kAppDiscoveryForOobe, features::kOobeNewRecommendApps});
    }
  }

  ~RecommendAppsScreenTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    recommend_apps_fetcher_factory_ =
        std::make_unique<ScopedTestRecommendAppsFetcherFactory>(
            base::BindRepeating(
                &RecommendAppsScreenTest::CreateRecommendAppsFetcher,
                base::Unretained(this)));

    recommend_apps_screen_ = WizardController::default_controller()
                                 ->GetScreen<RecommendAppsScreen>();
    recommend_apps_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &RecommendAppsScreenTest::HandleScreenExit, base::Unretained(this)));
  }
  void TearDownOnMainThread() override {
    recommend_apps_fetcher_ = nullptr;
    recommend_apps_fetcher_factory_.reset();

    OobeBaseTest::TearDownOnMainThread();
  }

  void ShowRecommendAppsScreen() {
    login_manager_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    LoginDisplayHost::default_host()->StartWizard(
        RecommendAppsScreenView::kScreenId);
  }

  void WaitForScreenExit() {
    if (screen_result_.has_value())
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ShowScreenAndExpectLoadingStep() {
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->defer_oobe_flow_finished_for_tests = true;
    ShowRecommendAppsScreen();
    OobeScreenWaiter(RecommendAppsScreenView::kScreenId).Wait();
    // Wait for loading screen.
    test::OobeJS()
        .CreateVisibilityWaiter(true, {kRecommendAppsId, "loadingDialog"})
        ->Wait();

    test::OobeJS().ExpectHiddenPath({kRecommendAppsId, "appsDialog"});
  }

  void ExpectAppSelectionStep() {
    test::OobeJS()
        .CreateVisibilityWaiter(true, {kRecommendAppsId, "appsDialog"})
        ->Wait();
    test::OobeJS().ExpectHiddenPath({kRecommendAppsId, "loadingDialog"});
  }

  bool WaitForAppListSize(const std::string& webview_path, int app_count) {
    std::string count_apps_script =
        "Array.from(document.getElementById('recommend-apps-container')"
        "     .querySelectorAll('.item'))"
        "     .map(i => i.getAttribute('data-packagename'));";

    std::string script = base::StringPrintf(
        "(function() {"
        "  var getAppCount = function() {"
        "    %s.executeScript({code: \"%s\"}, r => {"
        "      if (!r || !r[0] || r[0].length !== %d) {"
        "        setTimeout(getAppCount, 50);"
        "        return;"
        "      }"
        "      window.domAutomationController.send(true);"
        "    });"
        "  };"
        "  getAppCount();"
        "})();",
        webview_path.c_str(), count_apps_script.c_str(), app_count);

    // Wait for some apps to be shown
    bool result;
    return content::ExecuteScriptAndExtractBool(
               LoginDisplayHost::default_host()->GetOobeWebContents(), script,
               &result) &&
           result;
  }

  // Simulates click on the apps in the webview's app list.
  // The apps are expected to be passed in as a JavaScript array string.
  // For example ['app_package_name1', 'app_package_name_2']
  bool ToggleAppsSelection(const std::string& webview_path,
                           const std::string& package_names) {
    std::string toggle_apps_script = base::StringPrintf(
        "Array.from(document.getElementById('recommend-apps-container')"
        "     .querySelectorAll('.item'))"
        "     .filter(i => %s.includes(i.getAttribute('data-packagename')))"
        "     .forEach(i => i.querySelector('.image-picker').click());",
        package_names.c_str());

    std::string script = base::StringPrintf(
        "(function() {"
        "  %s.executeScript({code: \"%s\"},"
        "                   r => window.domAutomationController.send(true));"
        "})();",
        webview_path.c_str(), toggle_apps_script.c_str());

    bool result;
    return content::ExecuteScriptAndExtractBool(
               LoginDisplayHost::default_host()->GetOobeWebContents(), script,
               &result) &&
           result;
  }

  base::raw_ptr<RecommendAppsScreen> recommend_apps_screen_ = nullptr;
  absl::optional<RecommendAppsScreen::Result> screen_result_;
  base::raw_ptr<StubRecommendAppsFetcher> recommend_apps_fetcher_ = nullptr;

  LoginManagerMixin login_manager_{&mixin_host_};

 private:
  void HandleScreenExit(RecommendAppsScreen::Result result) {
    ASSERT_FALSE(screen_result_.has_value());
    screen_result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  std::unique_ptr<RecommendAppsFetcher> CreateRecommendAppsFetcher(
      RecommendAppsFetcherDelegate* delegate) {
    if (!GetParam())
      EXPECT_EQ(delegate, recommend_apps_screen_);
    EXPECT_FALSE(recommend_apps_fetcher_);

    auto fetcher = std::make_unique<StubRecommendAppsFetcher>(delegate);
    recommend_apps_fetcher_ = fetcher.get();
    return fetcher;
  }

  std::unique_ptr<ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;

  base::OnceClosure screen_exit_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest, BasicSelection) {
  ShowScreenAndExpectLoadingStep();

  if (IsNewRecommendedAppsEnabled()) {
    recommend_apps_fetcher_->SimulateSuccess();

    ExpectAppSelectionStep();
    test::OobeJS().CreateDisplayedWaiter(true, kAppsList)->Wait();
    test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
    test::OobeJS().ExpectDisabledPath(kInstallButton);
    test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
    test::OobeJS().ExpectEnabledPath(kSkipButton);

    test::OobeJS().ClickOnPath(kFirstAppCheckbox);
    test::OobeJS().ClickOnPath(kSecondAppCheckbox);
  } else {
    std::vector<FakeAppInfo> test_apps = {
        FakeAppInfo("test.app.foo.app1", "Test app 1"),
        FakeAppInfo("test.app.foo.app2", "Test app 2"),
        FakeAppInfo("test.app.foo.app3", "Test app 3")};
    recommend_apps_fetcher_->SimulateSuccess(test_apps);

    ExpectAppSelectionStep();

    const std::string webview_path = test::GetOobeElementPath(kWebviewUIPath);

    test::OobeJS().ExpectDisabledPath(kInstallButton);

    test::OobeJS().CreateDisplayedWaiter(true, kWebviewUIPath)->Wait();
    ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

    test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
    test::OobeJS().ExpectDisabledPath(kInstallButton);
    test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
    test::OobeJS().ExpectEnabledPath(kSkipButton);

    ASSERT_TRUE(ToggleAppsSelection(
        webview_path, "['test.app.foo.app1', 'test.app.foo.app2']"));
  }

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().TapOnPath(kInstallButton);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SELECTED, screen_result_.value());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);

  base::Value expected_pref_value(base::Value::Type::LIST);
  expected_pref_value.Append("test.app.foo.app1");
  expected_pref_value.Append("test.app.foo.app2");
  EXPECT_EQ(expected_pref_value, *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest, SelectionChange) {
  ShowScreenAndExpectLoadingStep();

  const std::string webview_path = test::GetOobeElementPath(kWebviewUIPath);

  if (IsNewRecommendedAppsEnabled()) {
    recommend_apps_fetcher_->SimulateSuccess();
    ExpectAppSelectionStep();
  } else {
    std::vector<FakeAppInfo> test_apps = {
        FakeAppInfo("test.app.foo.app1", "Test app 1"),
        FakeAppInfo("test.app.foo.app2", "Test app 2"),
        FakeAppInfo("test.app.foo.app3", "Test app 3")};
    recommend_apps_fetcher_->SimulateSuccess(test_apps);

    ExpectAppSelectionStep();

    test::OobeJS().ExpectDisabledPath(kInstallButton);

    test::OobeJS().CreateDisplayedWaiter(true, kWebviewUIPath)->Wait();
    ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));
  }

  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  if (IsNewRecommendedAppsEnabled()) {
    test::OobeJS().ClickOnPath(kFirstAppCheckbox);
    test::OobeJS().ClickOnPath(kSecondAppCheckbox);
  } else {
    ASSERT_TRUE(ToggleAppsSelection(
        webview_path, "['test.app.foo.app1', 'test.app.foo.app2']"));
  }

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  if (IsNewRecommendedAppsEnabled()) {
    test::OobeJS().ClickOnPath(kFirstAppCheckbox);
  } else {
    ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app1']"));
  }

  test::OobeJS().TapOnPath(kInstallButton);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SELECTED, screen_result_.value());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);

  base::Value expected_pref_value(base::Value::Type::LIST);
  expected_pref_value.Append("test.app.foo.app2");
  EXPECT_EQ(expected_pref_value, *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest, SkipWithSelectedApps) {
  ShowScreenAndExpectLoadingStep();

  const std::string webview_path = test::GetOobeElementPath(kWebviewUIPath);

  if (IsNewRecommendedAppsEnabled()) {
    recommend_apps_fetcher_->SimulateSuccess();
    ExpectAppSelectionStep();

    test::OobeJS().ExpectDisabledPath(kInstallButton);
  } else {
    std::vector<FakeAppInfo> test_apps = {
        FakeAppInfo("test.app.foo.app1", "Test app 1"),
        FakeAppInfo("test.app.foo.app2", "Test app 2"),
        FakeAppInfo("test.app.foo.app3", "Test app 3")};
    recommend_apps_fetcher_->SimulateSuccess(test_apps);
    ExpectAppSelectionStep();

    test::OobeJS().ExpectDisabledPath(kInstallButton);
    test::OobeJS().CreateDisplayedWaiter(true, kWebviewUIPath)->Wait();
    ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));
  }

  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  if (IsNewRecommendedAppsEnabled()) {
    test::OobeJS().ClickOnPath(kFirstAppCheckbox);
  } else {
    ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app2']"));
  }

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().TapOnPath(kSkipButton);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest, SkipWithNoAppsSelected) {
  ShowScreenAndExpectLoadingStep();

  const std::string webview_path = test::GetOobeElementPath(kWebviewUIPath);

  if (IsNewRecommendedAppsEnabled()) {
    recommend_apps_fetcher_->SimulateSuccess();
    ExpectAppSelectionStep();
  } else {
    std::vector<FakeAppInfo> test_apps = {
        FakeAppInfo("test.app.foo.app1", "Test app 1"),
        FakeAppInfo("test.app.foo.app2", "Test app 2"),
        FakeAppInfo("test.app.foo.app3", "Test app 3")};
    recommend_apps_fetcher_->SimulateSuccess(test_apps);

    ExpectAppSelectionStep();

    test::OobeJS().ExpectDisabledPath(kInstallButton);

    test::OobeJS().CreateDisplayedWaiter(true, kWebviewUIPath)->Wait();
    ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));
  }

  test::OobeJS().ExpectPathDisplayed(true, kInstallButton);
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().ExpectPathDisplayed(true, kSkipButton);
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  if (IsNewRecommendedAppsEnabled()) {
    test::OobeJS().ClickOnPath(kSecondAppCheckbox);
  } else {
    ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app2']"));
  }

  test::OobeJS().CreateEnabledWaiter(true, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  if (IsNewRecommendedAppsEnabled()) {
    test::OobeJS().ClickOnPath(kSecondAppCheckbox);
  } else {
    ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app2']"));
  }

  test::OobeJS().CreateEnabledWaiter(false, kInstallButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kSkipButton);

  test::OobeJS().TapOnPath(kSkipButton);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest,
                       InstallWithNoAppsSelectedDisabled) {
  ShowScreenAndExpectLoadingStep();

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1")};
  if (IsNewRecommendedAppsEnabled()) {
    recommend_apps_fetcher_->SimulateSuccess();
  } else {
    recommend_apps_fetcher_->SimulateSuccess(test_apps);
  }

  ExpectAppSelectionStep();

  const std::string webview_path = test::GetOobeElementPath(kWebviewUIPath);
  if (!IsNewRecommendedAppsEnabled()) {
    test::OobeJS().CreateDisplayedWaiter(true, kWebviewUIPath)->Wait();
    ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));
  }

  // The install button is expected to be disabled at this point. Check that
  // on install button click does nothing.
  test::OobeJS().ExpectDisabledPath(kInstallButton);
  test::OobeJS().TapOnPath(kInstallButton);
  ASSERT_FALSE(screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest, NoRecommendedApps) {
  ShowScreenAndExpectLoadingStep();
  if (IsNewRecommendedAppsEnabled()) {
    recommend_apps_fetcher_->SimulateSuccess(/*bad_response=*/true);
  } else {
    recommend_apps_fetcher_->SimulateSuccess(std::vector<FakeAppInfo>());
    ExpectAppSelectionStep();

    test::OobeJS().CreateDisplayedWaiter(true, kSkipButton)->Wait();
    test::OobeJS().ExpectEnabledPath(kSkipButton);
    test::OobeJS().ExpectDisabledPath(kInstallButton);

    test::OobeJS().TapOnPath(kSkipButton);
  }

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenTest, ParseError) {
  ShowScreenAndExpectLoadingStep();

  recommend_apps_fetcher_->SimulateParseError();

  ASSERT_TRUE(screen_result_.has_value());
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());
}

class RecommendAppsScreenManagedTest : public RecommendAppsScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_P(RecommendAppsScreenManagedTest, SkipDueToManagedUser) {
  // Force the sync screen to be shown so that OOBE isn't destroyed
  // right after login due to all screens being skipped.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();

  login_manager_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  if (!screen_result_.has_value()) {
    // Skip screens to the tested one.
    LoginDisplayHost::default_host()->StartWizard(
        RecommendAppsScreenView::kScreenId);
    WaitForScreenExit();
  }
  EXPECT_EQ(screen_result_.value(),
            RecommendAppsScreen::Result::NOT_APPLICABLE);
}

INSTANTIATE_TEST_SUITE_P(All, RecommendAppsScreenTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, RecommendAppsScreenManagedTest, testing::Bool());

}  // namespace
}  // namespace ash

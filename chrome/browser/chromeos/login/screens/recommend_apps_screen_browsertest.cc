// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

namespace {

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
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

class FakeRecommendAppsFetcher : public RecommendAppsFetcher {
 public:
  explicit FakeRecommendAppsFetcher(RecommendAppsFetcherDelegate* delegate)
      : delegate_(delegate) {}
  ~FakeRecommendAppsFetcher() override = default;

  bool started() const { return started_; }
  int retries() const { return retries_; }

  void SimulateSuccess(const std::vector<FakeAppInfo>& apps) {
    EXPECT_TRUE(started_);
    base::Value app_list(base::Value::Type::LIST);
    for (const auto& app : apps) {
      app_list.Append(app.ToValue());
    }
    delegate_->OnLoadSuccess(app_list);
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
  void Retry() override {
    EXPECT_TRUE(started_);
    ++retries_;
  }

 private:
  RecommendAppsFetcherDelegate* const delegate_;
  bool started_ = false;
  int retries_ = 0;
};

}  // namespace

class RecommendAppsScreenTest : public InProcessBrowserTest {
 public:
  RecommendAppsScreenTest() = default;
  ~RecommendAppsScreenTest() override = default;

  void SetUpOnMainThread() override {
    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);

    recommend_apps_fetcher_factory_ =
        std::make_unique<ScopedTestRecommendAppsFetcherFactory>(
            base::BindRepeating(
                &RecommendAppsScreenTest::CreateRecommendAppsFetcher,
                base::Unretained(this)));

    // Delete initial screen before we create the new screen, as the screen ctor
    // will bind to the handler.
    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(RecommendAppsScreenView::kScreenId);
    auto recommend_apps_screen = std::make_unique<RecommendAppsScreen>(
        GetOobeUI()->GetView<RecommendAppsScreenHandler>(),
        base::BindRepeating(&RecommendAppsScreenTest::HandleScreenExit,
                            base::Unretained(this)));
    recommend_apps_screen_ = recommend_apps_screen.get();
    WizardController::default_controller()
        ->screen_manager()
        ->SetScreenForTesting(std::move(recommend_apps_screen));

    InProcessBrowserTest::SetUpOnMainThread();
  }
  void TearDownOnMainThread() override {
    recommend_apps_fetcher_ = nullptr;
    recommend_apps_fetcher_factory_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForScreenExit() {
    if (screen_result_.has_value())
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
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

  RecommendAppsScreen* recommend_apps_screen_;
  base::Optional<RecommendAppsScreen::Result> screen_result_;
  FakeRecommendAppsFetcher* recommend_apps_fetcher_ = nullptr;

 private:
  void HandleScreenExit(RecommendAppsScreen::Result result) {
    ASSERT_FALSE(screen_result_.has_value());
    screen_result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  std::unique_ptr<RecommendAppsFetcher> CreateRecommendAppsFetcher(
      RecommendAppsFetcherDelegate* delegate) {
    EXPECT_EQ(delegate, recommend_apps_screen_);
    EXPECT_FALSE(recommend_apps_fetcher_);

    auto fetcher = std::make_unique<FakeRecommendAppsFetcher>(delegate);
    recommend_apps_fetcher_ = fetcher.get();
    return fetcher;
  }

  std::unique_ptr<ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;

  base::OnceClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, BasicSelection) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1"),
      FakeAppInfo("test.app.foo.app2", "Test app 2"),
      FakeAppInfo("test.app.foo.app3", "Test app 3")};
  recommend_apps_fetcher_->SimulateSuccess(test_apps);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().ExpectDisabledPath(install_button);

  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();
  ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

  test::OobeJS().ExpectPathDisplayed(true, install_button);
  test::OobeJS().ExpectDisabledPath(install_button);
  test::OobeJS().ExpectPathDisplayed(true, skip_button);
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  ASSERT_TRUE(ToggleAppsSelection(
      webview_path, "['test.app.foo.app1', 'test.app.foo.app2']"));

  test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  test::OobeJS().TapOnPath(install_button);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SELECTED, screen_result_.value());

  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);

  base::Value expected_pref_value(base::Value::Type::LIST);
  expected_pref_value.Append("test.app.foo.app1");
  expected_pref_value.Append("test.app.foo.app2");
  EXPECT_EQ(expected_pref_value, *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SelectionChange) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1"),
      FakeAppInfo("test.app.foo.app2", "Test app 2"),
      FakeAppInfo("test.app.foo.app3", "Test app 3")};
  recommend_apps_fetcher_->SimulateSuccess(test_apps);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().ExpectDisabledPath(install_button);

  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();
  ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

  test::OobeJS().ExpectPathDisplayed(true, install_button);
  test::OobeJS().ExpectDisabledPath(install_button);
  test::OobeJS().ExpectPathDisplayed(true, skip_button);
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  ASSERT_TRUE(ToggleAppsSelection(
      webview_path, "['test.app.foo.app1', 'test.app.foo.app2']"));

  test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app1']"));

  test::OobeJS().TapOnPath(install_button);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SELECTED, screen_result_.value());

  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);

  base::Value expected_pref_value(base::Value::Type::LIST);
  expected_pref_value.Append("test.app.foo.app2");
  EXPECT_EQ(expected_pref_value, *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SkipWithSelectedApps) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1"),
      FakeAppInfo("test.app.foo.app2", "Test app 2"),
      FakeAppInfo("test.app.foo.app3", "Test app 3")};
  recommend_apps_fetcher_->SimulateSuccess(test_apps);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().ExpectDisabledPath(install_button);

  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();
  ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

  test::OobeJS().ExpectPathDisplayed(true, install_button);
  test::OobeJS().ExpectDisabledPath(install_button);
  test::OobeJS().ExpectPathDisplayed(true, skip_button);
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app2']"));

  test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  test::OobeJS().TapOnPath(skip_button);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SkipWithNoAppsSelected) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1"),
      FakeAppInfo("test.app.foo.app2", "Test app 2"),
      FakeAppInfo("test.app.foo.app3", "Test app 3")};
  recommend_apps_fetcher_->SimulateSuccess(test_apps);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().ExpectDisabledPath(install_button);

  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();
  ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

  test::OobeJS().ExpectPathDisplayed(true, install_button);
  test::OobeJS().ExpectDisabledPath(install_button);
  test::OobeJS().ExpectPathDisplayed(true, skip_button);
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app2']"));

  test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  ASSERT_TRUE(ToggleAppsSelection(webview_path, "['test.app.foo.app2']"));

  test::OobeJS().CreateEnabledWaiter(false, install_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  test::OobeJS().TapOnPath(skip_button);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, InstallWithNoAppsSelected) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1")};
  recommend_apps_fetcher_->SimulateSuccess(test_apps);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();
  ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

  // The install button is expected to be disabled at this point. Send empty app
  // list directly to test handler behavior when install is triggered with no
  // apps selected.
  test::OobeJS().Evaluate("chrome.send('recommendAppsInstall', []);");

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, NoRecommendedApps) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  recommend_apps_fetcher_->SimulateSuccess(std::vector<FakeAppInfo>());

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().CreateDisplayedWaiter(true, skip_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectDisabledPath(install_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  test::OobeJS().TapOnPath(skip_button);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());

  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, ParseError) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  recommend_apps_fetcher_->SimulateParseError();

  ASSERT_TRUE(screen_result_.has_value());
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());
  EXPECT_EQ(0, recommend_apps_fetcher_->retries());
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, SkipOnLoadError) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  recommend_apps_fetcher_->SimulateLoadError();

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().CreateDisplayedWaiter(true, skip_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().CreateDisplayedWaiter(true, retry_button)->Wait();
  test::OobeJS().ExpectEnabledPath(retry_button);
  test::OobeJS().ExpectPathDisplayed(false, install_button);

  test::OobeJS().TapOnPath(skip_button);

  WaitForScreenExit();
  EXPECT_EQ(RecommendAppsScreen::Result::SKIPPED, screen_result_.value());
  EXPECT_EQ(0, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

IN_PROC_BROWSER_TEST_F(RecommendAppsScreenTest, RetryOnLoadError) {
  recommend_apps_screen_->Show();

  OobeScreenWaiter screen_waiter(RecommendAppsScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Wait for loading screen.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-loading"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-screen");

  recommend_apps_fetcher_->SimulateLoadError();

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps-screen", "recommend-apps-install-button"};
  const std::initializer_list<base::StringPiece> skip_button = {
      "recommend-apps-screen", "recommend-apps-skip-button"};
  const std::initializer_list<base::StringPiece> retry_button = {
      "recommend-apps-screen", "recommend-apps-retry-button"};

  test::OobeJS().CreateDisplayedWaiter(true, skip_button)->Wait();
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().CreateDisplayedWaiter(true, retry_button)->Wait();
  test::OobeJS().ExpectEnabledPath(retry_button);
  test::OobeJS().ExpectPathDisplayed(false, install_button);

  test::OobeJS().TapOnPath(retry_button);

  EXPECT_EQ(1, recommend_apps_fetcher_->retries());

  test::OobeJS()
      .CreateVisibilityWaiter(false, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectVisible("recommend-apps-loading");

  std::vector<FakeAppInfo> test_apps = {
      FakeAppInfo("test.app.foo.app1", "Test app 1")};
  recommend_apps_fetcher_->SimulateSuccess(test_apps);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps-screen"})
      ->Wait();
  test::OobeJS().ExpectHidden("recommend-apps-loading");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();
  ASSERT_TRUE(WaitForAppListSize(webview_path, test_apps.size()));

  test::OobeJS().ExpectPathDisplayed(true, install_button);
  test::OobeJS().ExpectDisabledPath(install_button);
  test::OobeJS().ExpectPathDisplayed(true, skip_button);
  test::OobeJS().ExpectEnabledPath(skip_button);
  test::OobeJS().ExpectPathDisplayed(false, retry_button);

  EXPECT_FALSE(screen_result_.has_value());
  EXPECT_EQ(1, recommend_apps_fetcher_->retries());

  const base::Value* fast_reinstall_packages =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->Get(
          arc::prefs::kArcFastAppReinstallPackages);
  ASSERT_TRUE(fast_reinstall_packages);
  EXPECT_EQ(base::Value(base::Value::Type::LIST), *fast_reinstall_packages);
}

}  // namespace chromeos

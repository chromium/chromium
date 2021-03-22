// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_allowlist_policy_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr char kExampleHost[] = "www.example.com";
constexpr char kExampleHost2[] = "www.example2.com";
constexpr char kExampleHost3[] = "www.example3.com";

class LoadFinishedWaiter : public content::WebContentsObserver {
 public:
  LoadFinishedWaiter(content::WebContents* web_contents, const GURL& url)
      : content::WebContentsObserver(web_contents), url_(url) {}

  ~LoadFinishedWaiter() override = default;

  // Delete copy constructor and copy assignment operator.
  LoadFinishedWaiter(const LoadFinishedWaiter&) = delete;
  LoadFinishedWaiter& operator=(const LoadFinishedWaiter&) = delete;

  void Wait() {
    if (did_finish_)
      return;
    run_loop_.Run();
  }

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  GURL url_;
  bool did_finish_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void LoadFinishedWaiter::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->GetParent() &&
      render_frame_host->GetLastCommittedURL() == url_) {
    did_finish_ = true;
    run_loop_.Quit();
  }
}
}  // namespace

class WebTimeLimitEnforcerThrottleTest : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUp() override;
  void TearDown() override;
  void SetUpOnMainThread() override;
  bool IsErrorPageBeingShownInWebContents(content::WebContents* tab);
  void AllowlistUrlRegx(const std::string& host);
  void AllowlistApp(const chromeos::app_time::AppId& app_id);
  void BlockWeb();
  chromeos::app_time::WebTimeLimitEnforcer* GetWebTimeLimitEnforcer();
  content::WebContents* InstallAndLaunchWebApp(const GURL& url,
                                               bool allowlisted_app);

 private:
  void UpdatePolicy();

  base::test::ScopedFeatureList scoped_feature_list_;

  chromeos::app_time::AppTimeLimitsAllowlistPolicyBuilder builder_;

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};
};

void WebTimeLimitEnforcerThrottleTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {features::kWebTimeLimits},
      /* disabled_features */ {});
  builder_.SetUp();
  MixinBasedInProcessBrowserTest::SetUp();
}

void WebTimeLimitEnforcerThrottleTest::TearDown() {
  builder_.Clear();
  MixinBasedInProcessBrowserTest::TearDown();
}

void WebTimeLimitEnforcerThrottleTest::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(embedded_test_server()->Started());

  logged_in_user_mixin_.LogInUser();
}

bool WebTimeLimitEnforcerThrottleTest::IsErrorPageBeingShownInWebContents(
    content::WebContents* tab) {
  const std::string command =
      "domAutomationController.send("
      "(document.getElementById('web-time-limit-block') != null)"
      "? (true) : (false));";

  bool value = false;
  content::ToRenderFrameHost target =
      content::ToRenderFrameHost(tab->GetMainFrame());
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      target, command, &value));
  return value;
}

void WebTimeLimitEnforcerThrottleTest::AllowlistUrlRegx(
    const std::string& url) {
  builder_.AppendToAllowlistUrlList(url);
  UpdatePolicy();
}

void WebTimeLimitEnforcerThrottleTest::AllowlistApp(
    const chromeos::app_time::AppId& app_id) {
  builder_.AppendToAllowlistAppList(app_id);
  UpdatePolicy();
}

void WebTimeLimitEnforcerThrottleTest::BlockWeb() {
  GetWebTimeLimitEnforcer()->OnWebTimeLimitReached(
      base::TimeDelta::FromHours(1));
}

chromeos::app_time::WebTimeLimitEnforcer*
WebTimeLimitEnforcerThrottleTest::GetWebTimeLimitEnforcer() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  chromeos::ChildUserService::TestApi child_user_service =
      chromeos::ChildUserService::TestApi(
          chromeos::ChildUserServiceFactory::GetForBrowserContext(
              browser_context));
  return child_user_service.web_time_enforcer();
}

content::WebContents* WebTimeLimitEnforcerThrottleTest::InstallAndLaunchWebApp(
    const GURL& url,
    bool allowlisted_app) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->title = base::UTF8ToUTF16(url.host());
  web_app_info->description = u"Web app";
  web_app_info->start_url = url;
  web_app_info->scope = url;
  web_app_info->open_as_window = true;
  web_app::AppId app_id =
      web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));

  if (allowlisted_app)
    AllowlistApp(chromeos::app_time::AppId(apps::mojom::AppType::kWeb, app_id));
  base::RunLoop().RunUntilIdle();

  // Add a tab to |browser()| and return the newly added WebContents.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void WebTimeLimitEnforcerThrottleTest::UpdatePolicy() {
  std::string policy_value;
  base::JSONWriter::Write(builder_.value(), &policy_value);

  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_perapptimelimitsallowlist()
      ->set_value(policy_value);

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();

  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->GetActiveUser());

  logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
      profile);
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WebBlockedBeforeBrowser) {
  // Alright let's block the browser.
  BlockWeb();
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(
      params.navigated_or_inserted_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WebBlockedAfterBrowser) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  // We don't expect an time limit block page to show yet.
  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));

  LoadFinishedWaiter waiter(web_contents, url);

  BlockWeb();

  waiter.Wait();

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       WebUnblockedAfterBlocked) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  // Alright let's block the browser.
  BlockWeb();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  LoadFinishedWaiter waiter(web_contents, url);

  GetWebTimeLimitEnforcer()->OnWebTimeLimitEnded();
  waiter.Wait();

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       AllowlistedURLNotBlocked) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");
  AllowlistUrlRegx(kExampleHost);

  // Alright let's block the browser.
  BlockWeb();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       BlockedURLAddedToAllowlist) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  // Alright let's block the browser.
  BlockWeb();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  LoadFinishedWaiter waiter(web_contents, url);

  AllowlistUrlRegx(kExampleHost);
  waiter.Wait();

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       AllowlistedSchemesNotBlockedHttp) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");

  BlockWeb();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  // Allowlist the http scheme and ensure that the page is not
  // blocked
  LoadFinishedWaiter waiter(web_contents, url);
  AllowlistUrlRegx("http://*");
  waiter.Wait();
  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       AllowlistedSchemesNotBlockedChrome) {
  GURL url = GURL("chrome://version");

  BlockWeb();
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  // Allowlist the chrome scheme and ensure that the page is not
  // blocked.
  LoadFinishedWaiter waiter(web_contents, url);
  AllowlistUrlRegx("chrome://*");
  waiter.Wait();
  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest,
                       AllowlistedWebAppInTabNotBlocked) {
  GURL web_app_url1 = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  GURL web_app_url2 = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  GURL normal_url = embedded_test_server()->GetURL(
      kExampleHost3, "/supervised_user/simple.html");

  content::WebContents* web_contents1 =
      InstallAndLaunchWebApp(web_app_url1, /* allowlist */ true);
  content::WebContents* web_contents2 =
      InstallAndLaunchWebApp(web_app_url2, /* allowlist */ false);

  NavigateParams params(browser(), normal_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  content::WebContents* web_contents3 = params.navigated_or_inserted_contents;

  LoadFinishedWaiter waiter1(web_contents1, web_app_url1);
  LoadFinishedWaiter waiter2(web_contents2, web_app_url2);
  LoadFinishedWaiter waiter3(web_contents3, normal_url);
  BlockWeb();
  waiter1.Wait();
  waiter2.Wait();
  waiter3.Wait();

  EXPECT_FALSE(IsErrorPageBeingShownInWebContents(web_contents1));
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents2));
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents3));
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest, WebContentTitleSet) {
  GURL url = embedded_test_server()->GetURL(kExampleHost,
                                            "/supervised_user/simple.html");
  NavigateParams params(browser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params);
  auto* web_contents = params.navigated_or_inserted_contents;
  auto* navigation_observer =
      chromeos::app_time::WebTimeNavigationObserver::FromWebContents(
          web_contents);
  std::u16string title = web_contents->GetTitle();
  EXPECT_EQ(title, navigation_observer->previous_title());

  LoadFinishedWaiter waiter(web_contents, url);
  BlockWeb();
  waiter.Wait();

  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));
  EXPECT_EQ(web_contents->GetTitle(), title);
}

IN_PROC_BROWSER_TEST_F(WebTimeLimitEnforcerThrottleTest, EnsureQueryIsCleared) {
  AllowlistUrlRegx(kExampleHost);
  BlockWeb();

  GURL url = embedded_test_server()->GetURL(kExampleHost2,
                                            "/supervised_user/simple.html");
  NavigateParams params1(browser(), url,
                         ui::PageTransition::PAGE_TRANSITION_LINK);
  // Navigates and waits for loading to finish.
  ui_test_utils::NavigateToURL(&params1);
  auto* web_contents = params1.navigated_or_inserted_contents;
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));

  GURL sneaky_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  GURL::Replacements replacements;
  replacements.SetQueryStr("var=chrome://settings");
  sneaky_url = sneaky_url.ReplaceComponents(replacements);
  NavigateParams params2(browser(), sneaky_url,
                         ui::PageTransition::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params2);
  web_contents = params2.navigated_or_inserted_contents;
  EXPECT_TRUE(IsErrorPageBeingShownInWebContents(web_contents));
}

// TODO(yilkal): Add AllowlistedSchemeNotBlocked test for  chrome://settings
// TODO(yilkal): Add test for blocked web contents without browser window.

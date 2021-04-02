// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/navigation_finished_waiter.h"
#include "chrome/browser/supervised_user/permission_request_creator_mock.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::WebContents;

namespace {

static const char* kExampleHost = "www.example.com";
static const char* kExampleHost2 = "www.example2.com";
static const char* kFamiliesHost = "families.google.com";
static const char* kIframeHost1 = "www.iframe1.com";
static const char* kIframeHost2 = "www.iframe2.com";

// Class to keep track of iframes created and destroyed.
// TODO(https://crbug.com/1188721): Can be replaced with
// WebContents::UnsafeFindFrameByFrameTreeNodeId.
class RenderFrameTracker : public content::WebContentsObserver {
 public:
  explicit RenderFrameTracker(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~RenderFrameTracker() override = default;

  // content::WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void FrameDeleted(int frame_tree_node_id) override;

  content::RenderFrameHost* GetHost(int frame_id) {
    if (!base::Contains(render_frame_hosts_, frame_id))
      return nullptr;
    return render_frame_hosts_[frame_id];
  }

 private:
  std::map<int, content::RenderFrameHost*> render_frame_hosts_;
};

void RenderFrameTracker::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  render_frame_hosts_[new_host->GetFrameTreeNodeId()] = new_host;
}

void RenderFrameTracker::FrameDeleted(int frame_tree_node_id) {
  if (!base::Contains(render_frame_hosts_, frame_tree_node_id))
    return;

  render_frame_hosts_.erase(frame_tree_node_id);
}

class InnerWebContentsAttachedWaiter : public content::WebContentsObserver {
 public:
  explicit InnerWebContentsAttachedWaiter(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  InnerWebContentsAttachedWaiter(const InnerWebContentsAttachedWaiter&) =
      delete;
  InnerWebContentsAttachedWaiter& operator=(
      const InnerWebContentsAttachedWaiter&) = delete;
  ~InnerWebContentsAttachedWaiter() override = default;

  // content::WebContentsObserver:
  void InnerWebContentsAttached(content::WebContents* inner_web_contents,
                                content::RenderFrameHost* render_frame_host,
                                bool is_full_page) override;

  void WaitForInnerWebContentsAttached();

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void InnerWebContentsAttachedWaiter::InnerWebContentsAttached(
    content::WebContents* inner_web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_full_page) {
  run_loop_.Quit();
}

void InnerWebContentsAttachedWaiter::WaitForInnerWebContentsAttached() {
  if (web_contents()->GetInnerWebContents().size() > 0u)
    return;
  run_loop_.Run();
}

}  // namespace

class SupervisedUserNavigationThrottleTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  SupervisedUserNavigationThrottleTest() = default;
  ~SupervisedUserNavigationThrottleTest() override = default;

  void SetUp() override;
  void SetUpOnMainThread() override;

  void BlockHost(const std::string& host) {
    SetManualFilterForHost(host, /* allowlist */ false);
  }

  void AllowlistHost(const std::string& host) {
    SetManualFilterForHost(host, /* allowlist */ true);
  }

  bool IsInterstitialBeingShownInMainFrame(Browser* browser);

  virtual chromeos::LoggedInUserMixin::LogInType GetLogInType() {
    return chromeos::LoggedInUserMixin::LogInType::kChild;
  }

 private:
  void SetManualFilterForHost(const std::string& host, bool allowlist) {
    Profile* profile = browser()->profile();
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(
            profile->GetProfileKey());

    const base::DictionaryValue* local_settings =
        settings_service->LocalSettingsForTest();
    std::unique_ptr<base::DictionaryValue> dict_to_insert;

    if (local_settings->HasKey(
            supervised_users::kContentPackManualBehaviorHosts)) {
      const base::DictionaryValue* dict_value;

      local_settings->GetDictionary(
          supervised_users::kContentPackManualBehaviorHosts, &dict_value);

      std::unique_ptr<base::Value> clone =
          std::make_unique<base::Value>(dict_value->Clone());

      dict_to_insert = base::DictionaryValue::From(std::move(clone));
    } else {
      dict_to_insert = std::make_unique<base::DictionaryValue>();
    }

    dict_to_insert->SetKey(host, base::Value(allowlist));
    settings_service->SetLocalSetting(
        supervised_users::kContentPackManualBehaviorHosts,
        std::move(dict_to_insert));
  }

  std::unique_ptr<chromeos::LoggedInUserMixin> logged_in_user_mixin_;
};

bool SupervisedUserNavigationThrottleTest::IsInterstitialBeingShownInMainFrame(
    Browser* browser) {
  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser, &title);
  return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
             content::PAGE_TYPE_ERROR &&
         title == u"Site blocked";
}

void SupervisedUserNavigationThrottleTest::SetUp() {
  // Polymorphically initiate logged_in_user_mixin_.
  logged_in_user_mixin_ = std::make_unique<chromeos::LoggedInUserMixin>(
      &mixin_host_, GetLogInType(), embedded_test_server(), this);
  MixinBasedInProcessBrowserTest::SetUp();
}

void SupervisedUserNavigationThrottleTest::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(embedded_test_server()->Started());

  logged_in_user_mixin_->LogInUser();
}

// Tests that navigating to a blocked page simply fails if there is no
// SupervisedUserNavigationObserver.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       NoNavigationObserverBlock) {
  Profile* profile = browser()->profile();
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackDefaultFilteringBehavior,
      std::unique_ptr<base::Value>(
          new base::Value(SupervisedUserURLFilter::BLOCK)));

  std::unique_ptr<WebContents> web_contents(
      WebContents::Create(WebContents::CreateParams(profile)));
  NavigationController& controller = web_contents->GetController();
  content::TestNavigationObserver observer(web_contents.get());
  controller.LoadURL(GURL("http://www.example.com"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(observer.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       BlockMainFrameWithInterstitial) {
  BlockHost(kExampleHost2);

  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       DontBlockSubFrame) {
  BlockHost(kExampleHost2);
  BlockHost(kIframeHost2);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  // Both iframes (from allowed host iframe1.com as well as from blocked host
  // iframe2.com) should be loaded normally, since we don't filter iframes
  // (yet) - see crbug.com/651115.
  bool loaded1 = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, "loaded1()", &loaded1));
  EXPECT_TRUE(loaded1);
  bool loaded2 = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, "loaded2()", &loaded2));
  EXPECT_TRUE(loaded2);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       AllowFamiliesDotGoogleDotComAccess) {
  // Simulate families.google.com being set in the blocklist.
  BlockHost(kFamiliesHost);
  // TODO(https://crbug.com/1174695): This test is relying on a production
  // service being available.  It should be probably be a TAST test instead of a
  // browsertest.
  const GURL kFamiliesDotGoogleDotComUrl =
      GURL("https://families.google.com/families");

  ui_test_utils::NavigateToURL(browser(), kFamiliesDotGoogleDotComUrl);

  // Get the top level WebContents.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(contents->GetLastCommittedURL(), kFamiliesDotGoogleDotComUrl);

  // families.google.com should not be blocked.
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

class SupervisedUserIframeFilterTest
    : public SupervisedUserNavigationThrottleTest {
 protected:
  SupervisedUserIframeFilterTest() = default;
  ~SupervisedUserIframeFilterTest() override = default;

  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  std::vector<int> GetBlockedFrames();
  const GURL& GetBlockedFrameURL(int frame_id);
  bool IsInterstitialBeingShownInFrame(int frame_id);
  bool IsAskPermissionButtonBeingShown(int frame_id);
  void RequestPermissionFromFrame(int frame_id);
  void WaitForNavigationFinished(int frame_id, const GURL& url);

  PermissionRequestCreatorMock* permission_creator() {
    return permission_creator_;
  }

  RenderFrameTracker* tracker() { return tracker_.get(); }

 private:
  bool RunCommandAndGetBooleanFromFrame(int frame_id,
                                        const std::string& command);

  std::unique_ptr<RenderFrameTracker> tracker_;
  PermissionRequestCreatorMock* permission_creator_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

void SupervisedUserIframeFilterTest::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      supervised_users::kSupervisedUserIframeFilter);
  SupervisedUserNavigationThrottleTest::SetUp();
}

void SupervisedUserIframeFilterTest::SetUpOnMainThread() {
  SupervisedUserNavigationThrottleTest::SetUpOnMainThread();

  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  std::unique_ptr<PermissionRequestCreator> creator =
      std::make_unique<PermissionRequestCreatorMock>(browser()->profile());
  permission_creator_ =
      static_cast<PermissionRequestCreatorMock*>(creator.get());
  permission_creator_->SetEnabled();
  service->SetPrimaryPermissionCreatorForTest(std::move(creator));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  tracker_ = std::make_unique<RenderFrameTracker>(tab);
}

void SupervisedUserIframeFilterTest::TearDownOnMainThread() {
  tracker_.reset();
  SupervisedUserNavigationThrottleTest::TearDownOnMainThread();
}

std::vector<int> SupervisedUserIframeFilterTest::GetBlockedFrames() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  const auto& interstitials = navigation_observer->interstitials_for_test();

  std::vector<int> blocked_frames;
  blocked_frames.reserve(interstitials.size());

  for (const auto& elem : interstitials)
    blocked_frames.push_back(elem.first);

  return blocked_frames;
}

const GURL& SupervisedUserIframeFilterTest::GetBlockedFrameURL(int frame_id) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  const auto& interstitials = navigation_observer->interstitials_for_test();
  DCHECK(base::Contains(interstitials, frame_id));
  return interstitials.at(frame_id)->url();
}

bool SupervisedUserIframeFilterTest::IsInterstitialBeingShownInFrame(
    int frame_id) {
  std::string command =
      "domAutomationController.send("
      "(document.getElementsByClassName('supervised-user-block') != null) "
      "? (true) : (false));";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsAskPermissionButtonBeingShown(
    int frame_id) {
  std::string command =
      "domAutomationController.send("
      "(document.getElementById('request-access-button').hidden"
      "? (false) : (true)));";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

void SupervisedUserIframeFilterTest::RequestPermissionFromFrame(int frame_id) {
  auto* render_frame_host = tracker()->GetHost(frame_id);
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsRenderFrameLive());
  std::string command = "sendCommand(\'request\')";
  ASSERT_TRUE(content::ExecuteScript(
      content::ToRenderFrameHost(render_frame_host), command));
}

void SupervisedUserIframeFilterTest::WaitForNavigationFinished(
    int frame_id,
    const GURL& url) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationFinishedWaiter waiter(tab, frame_id, url);
  waiter.Wait();
}

bool SupervisedUserIframeFilterTest::RunCommandAndGetBooleanFromFrame(
    int frame_id,
    const std::string& command) {
  // First check that SupervisedUserNavigationObserver believes that there is
  // an error page in the frame with frame tree node id |frame_id|.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  auto& interstitials = navigation_observer->interstitials_for_test();

  if (!base::Contains(interstitials, frame_id))
    return false;

  auto* render_frame_host = tracker()->GetHost(frame_id);
  DCHECK(render_frame_host->IsRenderFrameLive());

  bool value = false;
  auto target = content::ToRenderFrameHost(render_frame_host);
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      target, command, &value));
  return value;
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest, BlockSubFrame) {
  BlockHost(kIframeHost2);
  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  // The first iframe's source is |kIframeHost1| and it is not blocked. It will
  // successfully load.
  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);

  int blocked_frame_id = blocked[0];

  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id));

  permission_creator()->SetPermissionResult(true);
  RequestPermissionFromFrame(blocked_frame_id);
  EXPECT_EQ(permission_creator()->url_requests().size(), 1u);
  std::string requested_host = permission_creator()->url_requests()[0].host();

  EXPECT_EQ(requested_host, kIframeHost2);

  WaitForNavigationFinished(blocked[0],
                            permission_creator()->url_requests()[0]);

  EXPECT_FALSE(IsInterstitialBeingShownInFrame(blocked_frame_id));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest, BlockMultipleSubFrames) {
  BlockHost(kIframeHost1);
  BlockHost(kIframeHost2);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 2u);

  int blocked_frame_id_1 = blocked[0];
  GURL blocked_frame_url_1 = GetBlockedFrameURL(blocked_frame_id_1);

  int blocked_frame_id_2 = blocked[1];
  GURL blocked_frame_url_2 = GetBlockedFrameURL(blocked_frame_id_2);

  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id_1));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  RequestPermissionFromFrame(blocked_frame_id_1);
  RequestPermissionFromFrame(blocked_frame_id_2);

  EXPECT_EQ(permission_creator()->url_requests().size(), 2u);
  EXPECT_EQ(permission_creator()->url_requests()[0], GURL(blocked_frame_url_1));
  EXPECT_EQ(permission_creator()->url_requests()[1], GURL(blocked_frame_url_2));

  NavigationFinishedWaiter waiter1(tab, blocked_frame_id_1,
                                   blocked_frame_url_1);
  NavigationFinishedWaiter waiter2(tab, blocked_frame_id_2,
                                   blocked_frame_url_2);

  permission_creator()->HandleDelayedRequests();

  waiter1.Wait();
  waiter2.Wait();

  DCHECK_EQ(GetBlockedFrames().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest, TestBackButton) {
  BlockHost(kIframeHost1);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);

  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  RequestPermissionFromFrame(blocked[0]);

  std::string command =
      "domAutomationController.send("
      "(document.getElementById('back-button').hidden));";

  auto* render_frame_host = tracker()->GetHost(blocked[0]);
  DCHECK(render_frame_host->IsRenderFrameLive());
  bool value = false;
  auto target = content::ToRenderFrameHost(render_frame_host);
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      target, command, &value));

  // Back button should be hidden in iframes.
  EXPECT_TRUE(value);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest,
                       TestBackButtonMainFrame) {
  BlockHost(kExampleHost);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);

  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  RequestPermissionFromFrame(blocked[0]);

  std::string command =
      "domAutomationController.send("
      "(document.getElementById('back-button').hidden));";
  auto* render_frame_host = tracker()->GetHost(blocked[0]);
  DCHECK(render_frame_host->IsRenderFrameLive());

  bool value = false;
  auto target = content::ToRenderFrameHost(render_frame_host);
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      target, command, &value));

  // Back button should not be hidden in main frame.
  EXPECT_FALSE(value);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest,
                       AllowlistedMainFrameDenylistedIframe) {
  AllowlistHost(kExampleHost);
  BlockHost(kIframeHost1);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);
  EXPECT_EQ(kIframeHost1, GetBlockedFrameURL(blocked[0]).host());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest,
                       RememberAlreadyRequestedHosts) {
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked_frames = GetBlockedFrames();
  EXPECT_EQ(blocked_frames.size(), 1u);

  // Expect that request permission button is shown.
  EXPECT_TRUE(IsAskPermissionButtonBeingShown(blocked_frames[0]));

  // Delay approval/denial by parent.
  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  // Request permission.
  RequestPermissionFromFrame(blocked_frames[0]);

  // Navigate to another allowed url.
  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  ui_test_utils::NavigateToURL(browser(), blocked_url);
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  // Navigate back to the blocked url.
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  // Error page is being shown, but "Ask Permission" button is not being shown.
  EXPECT_FALSE(IsAskPermissionButtonBeingShown(blocked_frames[0]));

  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(active_contents);
  ASSERT_NE(navigation_observer, nullptr);

  EXPECT_TRUE(base::Contains(navigation_observer->requested_hosts_for_test(),
                             kExampleHost));

  NavigationFinishedWaiter waiter(
      active_contents, active_contents->GetMainFrame()->GetFrameTreeNodeId(),
      blocked_url);
  permission_creator()->HandleDelayedRequests();
  waiter.Wait();

  EXPECT_FALSE(base::Contains(navigation_observer->requested_hosts_for_test(),
                              kExampleHost));

  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIframeFilterTest,
                       IframesWithSameDomainAsMainFrameAllowed) {
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  SupervisedUserURLFilter* filter = service->GetURLFilter();

  // Set the default behavior to block.
  filter->SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  // The async checker will make rpc calls to check if the url should be
  // blocked or not. This may cause flakiness.
  filter->ClearAsyncURLChecker();

  base::RunLoop().RunUntilIdle();

  // Allows |www.example.com|.
  AllowlistHost(kExampleHost);

  // |with_frames_same_domain.html| contains subframes with "a.example.com" and
  // "b.example.com", and "c.example2.com" urls.
  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes_same_domain.html");

  ui_test_utils::NavigateToURL(browser(), allowed_url);
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked_frames = GetBlockedFrames();
  EXPECT_EQ(blocked_frames.size(), 1u);
  EXPECT_EQ(GetBlockedFrameURL(blocked_frames[0]).host(), "www.c.example2.com");
}

class SupervisedUserNavigationThrottleNotSupervisedTest
    : public SupervisedUserNavigationThrottleTest {
 protected:
  SupervisedUserNavigationThrottleNotSupervisedTest() = default;
  ~SupervisedUserNavigationThrottleNotSupervisedTest() override = default;

  chromeos::LoggedInUserMixin::LogInType GetLogInType() override {
    return chromeos::LoggedInUserMixin::LogInType::kRegular;
  }
};

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleNotSupervisedTest,
                       DontBlock) {
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);
  // Even though the URL is marked as blocked, the load should go through, since
  // the user isn't supervised.
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

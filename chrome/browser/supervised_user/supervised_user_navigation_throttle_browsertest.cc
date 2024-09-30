// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/browser/permission_request_creator_mock.h"
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/features_testutils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using content::NavigationController;
using content::WebContents;

static const char* kExampleHost = "www.example.com";
static const char* kExampleHost2 = "www.example2.com";
static const char* kStrippedExampleHost = "example.com";
static const char* kFamiliesHost = "families.google.com";
static const char* kIframeHost1 = "www.iframe1.com";
static const char* kIframeHost2 = "www.iframe2.com";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kLocalUrlAccessCommand[] = "requestUrlAccessLocal";
#endif
constexpr char kRemoteUrlAccessCommand[] = "requestUrlAccessRemote";

class ThrottleTestParam {
 public:
  enum class FeatureStatus : bool {
    // ClassifyUrlOnProcessResponseEvent feature disabled: uses
    // SupervisedUserNavigationThrottle
    kDisabled = false,
    // ClassifyUrlOnProcessResponseEvent feature enabled: uses
    // supervised_user::ClassifyUrlNavigationThrottle
    kEnabled = true,
  };
  static auto Values() {
    return ::testing::Values(FeatureStatus::kDisabled, FeatureStatus::kEnabled);
  }
  static std::string ToString(FeatureStatus status) {
    switch (status) {
      case FeatureStatus::kDisabled:
        return "SupervisedUserNavigationThrottle";
      case FeatureStatus::kEnabled:
        return "ClassifyUrlNavigationThrottle";
      default:
        NOTREACHED();
    }
  }
  static void InitFeatureList(base::test::ScopedFeatureList& feature_list,
                              FeatureStatus status) {
    feature_list.InitWithFeatureState(
        supervised_user::kClassifyUrlOnProcessResponseEvent,
        status == FeatureStatus::kEnabled);
  }
};

// Class to keep track of iframes created and destroyed.
// TODO(crbug.com/40173416): Can be replaced with
// WebContents::UnsafeFindFrameByFrameTreeNodeId.
class RenderFrameTracker : public content::WebContentsObserver {
 public:
  explicit RenderFrameTracker(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~RenderFrameTracker() override = default;

  // content::WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;

  content::RenderFrameHost* GetHost(content::FrameTreeNodeId frame_id) {
    if (!base::Contains(render_frame_hosts_, frame_id)) {
      return nullptr;
    }
    return render_frame_hosts_[frame_id];
  }

 private:
  std::map<content::FrameTreeNodeId,
           raw_ptr<content::RenderFrameHost, CtnExperimental>>

      render_frame_hosts_;
};

void RenderFrameTracker::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  render_frame_hosts_[new_host->GetFrameTreeNodeId()] = new_host;
}

void RenderFrameTracker::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  if (!base::Contains(render_frame_hosts_, frame_tree_node_id)) {
    return;
  }

  render_frame_hosts_.erase(frame_tree_node_id);
}

// Helper class to wait for a particular navigation in a particular render
// frame in tests.
class NavigationFinishedWaiter : public content::WebContentsObserver {
 public:
  NavigationFinishedWaiter(content::WebContents* web_contents,
                           content::FrameTreeNodeId frame_id,
                           const GURL& url);
  NavigationFinishedWaiter(const NavigationFinishedWaiter&) = delete;
  NavigationFinishedWaiter& operator=(const NavigationFinishedWaiter&) = delete;
  ~NavigationFinishedWaiter() override = default;

  void Wait();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  content::FrameTreeNodeId frame_id_;
  GURL url_;
  bool did_finish_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

NavigationFinishedWaiter::NavigationFinishedWaiter(
    content::WebContents* web_contents,
    content::FrameTreeNodeId frame_id,
    const GURL& url)
    : content::WebContentsObserver(web_contents),
      frame_id_(frame_id),
      url_(url) {}

void NavigationFinishedWaiter::Wait() {
  if (did_finish_) {
    return;
  }
  run_loop_.Run();
}

void NavigationFinishedWaiter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  if (navigation_handle->GetFrameTreeNodeId() != frame_id_ ||
      navigation_handle->GetURL() != url_) {
    return;
  }

  did_finish_ = true;
  run_loop_.Quit();
}

class SupervisedUserNavigationThrottleTestBase
    : public MixinBasedInProcessBrowserTest {
 protected:
  explicit SupervisedUserNavigationThrottleTestBase(
      supervised_user::SupervisionMixin::SignInMode sign_in_mode)
      : supervision_mixin_(mixin_host_,
                           this,
                           embedded_test_server(),
                           {
                               .sign_in_mode = sign_in_mode,
                               .embedded_test_server_options =
                                   {.resolver_rules_map_host_list =
                                        "*.example.com, *.example2.com, "
                                        "example.com, example2.com, "
                                        "*.families.google.com, "
                                        "*.iframe1.com, *.iframe2.com, "
                                        "iframe1.com, iframe2.com"},
                           }),
        prerender_helper_(base::BindRepeating(
            &SupervisedUserNavigationThrottleTestBase::web_contents,
            base::Unretained(this))) {}
  ~SupervisedUserNavigationThrottleTestBase() override = default;

  void SetUp() override;
  void SetUpOnMainThread() override;

  void BlockHost(const std::string& host) {
    supervised_user_test_util::SetManualFilterForHost(browser()->profile(),
                                                      host,
                                                      /*allowlist=*/false);
  }

  void AllowlistHost(const std::string& host) {
    supervised_user_test_util::SetManualFilterForHost(browser()->profile(),
                                                      host, /*allowlist=*/true);
  }

  bool IsInterstitialBeingShownInMainFrame(Browser* browser);

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

 private:
  supervised_user::SupervisionMixin supervision_mixin_;
  content::test::PrerenderTestHelper prerender_helper_;
};

bool SupervisedUserNavigationThrottleTestBase::
    IsInterstitialBeingShownInMainFrame(Browser* browser) {
  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser, &title);
  return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
             content::PAGE_TYPE_ERROR &&
         title == u"Site blocked";
}

void SupervisedUserNavigationThrottleTestBase::SetUp() {
  prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
  MixinBasedInProcessBrowserTest::SetUp();
}

void SupervisedUserNavigationThrottleTestBase::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(embedded_test_server()->Started());
}

class SupervisedUserNavigationThrottleTest
    : public SupervisedUserNavigationThrottleTestBase,
      ::testing::WithParamInterface<ThrottleTestParam::FeatureStatus> {
 protected:
  SupervisedUserNavigationThrottleTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {}
  ~SupervisedUserNavigationThrottleTest() override = default;
};

class SupervisedUserNavigationThrottleWithPrerenderingTest
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<
          std::tuple<std::string, ThrottleTestParam::FeatureStatus>> {
 protected:
  SupervisedUserNavigationThrottleWithPrerenderingTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {
    ThrottleTestParam::InitFeatureList(scoped_feature_list_,
                                       std::get<1>(GetParam()));
  }
  ~SupervisedUserNavigationThrottleWithPrerenderingTest() override = default;

  static std::string GetTargetHint() { return std::get<0>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationThrottleWithPrerenderingTest,
    testing::Combine(testing::Values("_self", "_blank"),
                     ThrottleTestParam::Values()),
    [](const testing::TestParamInfo<
        std::tuple<std::string, ThrottleTestParam::FeatureStatus>>& info) {
      return base::StrCat(
          {"WithTargetHint", std::get<0>(info.param), "WithThrottle",
           ThrottleTestParam::ToString(std::get<1>(info.param))});
    });

// Tests that prerendering fails in supervised user mode.
#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40201321): Flaky on ChromeOS.
#define MAYBE_DisallowPrerendering DISABLED_DisallowPrerendering
#else
#define MAYBE_DisallowPrerendering DisallowPrerendering
#endif
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationThrottleWithPrerenderingTest,
                       MAYBE_DisallowPrerendering) {
  const GURL initial_url = embedded_test_server()->GetURL("/simple.html");
  const GURL allowed_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  prerender_helper().NavigatePrimaryPage(initial_url);

  // If throttled, the prerendered navigation should not have started and we
  // should not be requesting corresponding resources.
  content::test::PrerenderHostCreationWaiter host_creation_waiter;
  prerender_helper().AddPrerendersAsync(
      {allowed_url}, /*eagerness=*/std::nullopt, GetTargetHint());
  content::FrameTreeNodeId host_id = host_creation_waiter.Wait();
  auto* prerender_web_contents =
      content::WebContents::FromFrameTreeNodeId(host_id);
  content::test::PrerenderHostObserver host_observer(*prerender_web_contents,
                                                     host_id);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(0, prerender_helper().GetRequestCount(allowed_url));

  // Regular navigation should proceed, however.
  content::TestNavigationObserver observer(
      web_contents(), content::MessageLoopRunner::QuitMode::IMMEDIATE,
      /*ignore_uncommitted_navigations*/ false);
  prerender_helper().NavigatePrimaryPage(allowed_url);
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(allowed_url, observer.last_navigation_url());
  EXPECT_EQ(1, prerender_helper().GetRequestCount(allowed_url));
}

// Tests that navigating to a blocked page simply fails if there is no
// SupervisedUserNavigationObserver.
IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       NoNavigationObserverBlock) {
  Profile* profile = browser()->profile();
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(
          static_cast<int>(supervised_user::FilteringBehavior::kBlock)));

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

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       DontBlockSubFrame) {
  BlockHost(kExampleHost2);
  BlockHost(kIframeHost2);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  // Both iframes (from allowed host iframe1.com as well as from blocked host
  // iframe2.com) should be loaded normally, since we don't filter iframes
  // (yet) - see crbug.com/651115.
  EXPECT_TRUE(content::EvalJs(tab, "loaded1()").ExtractBool());
  EXPECT_TRUE(content::EvalJs(tab, "loaded2()").ExtractBool());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleTest,
                       AllowFamiliesDotGoogleDotComAccess) {
  // Simulate families.google.com being set in the blocklist.
  BlockHost(kFamiliesHost);

  // A production endpoint is used here because a Tast test would be too
  // expensive to be used for this specific case.
  const GURL kFamiliesDotGoogleDotComUrl =
      GURL("https://families.google.com/families");

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kFamiliesDotGoogleDotComUrl));

  // Get the top level WebContents.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(contents->GetLastCommittedURL(), kFamiliesDotGoogleDotComUrl);

  // families.google.com should not be blocked.
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

class SupervisedUserIframeFilterTest
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<
          std::tuple<supervised_user::testing::LocalWebApprovalsTestCase,
                     ThrottleTestParam::FeatureStatus>> {
 protected:
  SupervisedUserIframeFilterTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {
    ThrottleTestParam::InitFeatureList(throttle_feature_list_,
                                       std::get<1>(GetParam()));
  }

  ~SupervisedUserIframeFilterTest() override = default;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  std::vector<content::FrameTreeNodeId> GetBlockedFrames();
  const GURL& GetBlockedFrameURL(content::FrameTreeNodeId frame_id);
  bool IsInterstitialBeingShownInFrame(content::FrameTreeNodeId frame_id);
  bool IsRemoteApprovalsButtonBeingShown(content::FrameTreeNodeId frame_id);
  bool IsLocalApprovalsButtonBeingShown(content::FrameTreeNodeId frame_id);
  bool IsBlockReasonBeingShown(content::FrameTreeNodeId frame_id);
  bool IsDetailsLinkBeingShown(content::FrameTreeNodeId frame_id);
  void CheckPreferredApprovalButton(content::FrameTreeNodeId frame_id);
  bool IsLocalApprovalsInsteadButtonBeingShown(
      content::FrameTreeNodeId frame_id);
  void SendCommandToFrame(const std::string& command_name,
                          content::FrameTreeNodeId frame_id);
  void WaitForNavigationFinished(content::FrameTreeNodeId frame_id,
                                 const GURL& url);
  bool IsLocalWebApprovalsEnabled() const;

  supervised_user::PermissionRequestCreatorMock* permission_creator() {
    return permission_creator_;
  }

  RenderFrameTracker* tracker() { return tracker_.get(); }

 private:
  static supervised_user::testing::LocalWebApprovalsTestCase
  GetLocalWebApprovalsSupportTestParam() {
    return std::get<0>(GetParam());
  }

 private:
  bool RunCommandAndGetBooleanFromFrame(content::FrameTreeNodeId frame_id,
                                        const std::string& command);

  std::unique_ptr<RenderFrameTracker> tracker_;
  raw_ptr<supervised_user::PermissionRequestCreatorMock, DanglingUntriaged>
      permission_creator_;

  // Each feature is enabled within its own feature list.
  std::unique_ptr<base::test::ScopedFeatureList>
      local_web_approvals_test_support_{
          GetLocalWebApprovalsSupportTestParam().MakeFeatureList()};
  base::test::ScopedFeatureList throttle_feature_list_;
};

void SupervisedUserIframeFilterTest::SetUpOnMainThread() {
  SupervisedUserNavigationThrottleTestBase::SetUpOnMainThread();

  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  CHECK(settings_service);
  std::unique_ptr<supervised_user::PermissionRequestCreator> creator =
      std::make_unique<supervised_user::PermissionRequestCreatorMock>(
          *settings_service);
  permission_creator_ =
      static_cast<supervised_user::PermissionRequestCreatorMock*>(
          creator.get());
  permission_creator_->SetEnabled();
  service->remote_web_approvals_manager().ClearApprovalRequestsCreators();
  service->remote_web_approvals_manager().AddApprovalRequestCreator(
      std::move(creator));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  tracker_ = std::make_unique<RenderFrameTracker>(tab);
}

void SupervisedUserIframeFilterTest::TearDownOnMainThread() {
  tracker_.reset();
  SupervisedUserNavigationThrottleTestBase::TearDownOnMainThread();
}

std::vector<content::FrameTreeNodeId>
SupervisedUserIframeFilterTest::GetBlockedFrames() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  const auto& interstitials = navigation_observer->interstitials_for_test();

  std::vector<content::FrameTreeNodeId> blocked_frames;
  blocked_frames.reserve(interstitials.size());

  for (const auto& elem : interstitials) {
    blocked_frames.push_back(elem.first);
  }

  return blocked_frames;
}

const GURL& SupervisedUserIframeFilterTest::GetBlockedFrameURL(
    content::FrameTreeNodeId frame_id) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  const auto& interstitials = navigation_observer->interstitials_for_test();
  DCHECK(base::Contains(interstitials, frame_id));
  return interstitials.at(frame_id)->url();
}

bool SupervisedUserIframeFilterTest::IsInterstitialBeingShownInFrame(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "document.getElementsByClassName('supervised-user-block') != null";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsBlockReasonBeingShown(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "getComputedStyle(document.getElementById('block-reason')).display !== "
      "\"none\"";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsDetailsLinkBeingShown(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "getComputedStyle(document.getElementById('block-reason-show-details-"
      "link')).display !== \"none\"";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsRemoteApprovalsButtonBeingShown(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "!document.getElementById('remote-approvals-button').hidden";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsLocalApprovalsButtonBeingShown(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "!document.getElementById('local-approvals-button').hidden";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

void SupervisedUserIframeFilterTest::CheckPreferredApprovalButton(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "document.getElementById('local-approvals-button').classList.contains("
      "'primary-button') &&"
      " !document.getElementById('local-approvals-button').classList."
      "contains('secondary-button') &&"
      " document.getElementById('remote-approvals-button').classList."
      "contains('secondary-button') &&"
      " !document.getElementById('remote-approvals-button').classList."
      "contains('primary-button');";
  ASSERT_TRUE(RunCommandAndGetBooleanFromFrame(frame_id, command));
}

bool SupervisedUserIframeFilterTest::IsLocalApprovalsInsteadButtonBeingShown(
    content::FrameTreeNodeId frame_id) {
  std::string command =
      "!document.getElementById('local-approvals-remote-request-sent-button')."
      "hidden";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

void SupervisedUserIframeFilterTest::SendCommandToFrame(
    const std::string& command_name,
    content::FrameTreeNodeId frame_id) {
  auto* render_frame_host = tracker()->GetHost(frame_id);
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsRenderFrameLive());
  std::string command = base::StrCat({"sendCommand(\'", command_name, "\')"});
  ASSERT_TRUE(
      content::ExecJs(content::ToRenderFrameHost(render_frame_host), command));
}

void SupervisedUserIframeFilterTest::WaitForNavigationFinished(
    content::FrameTreeNodeId frame_id,
    const GURL& url) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationFinishedWaiter waiter(tab, frame_id, url);
  waiter.Wait();
}

bool SupervisedUserIframeFilterTest::RunCommandAndGetBooleanFromFrame(
    content::FrameTreeNodeId frame_id,
    const std::string& command) {
  // First check that SupervisedUserNavigationObserver believes that there is
  // an error page in the frame with frame tree node id |frame_id|.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  auto& interstitials = navigation_observer->interstitials_for_test();

  if (!base::Contains(interstitials, frame_id)) {
    return false;
  }

  auto* render_frame_host = tracker()->GetHost(frame_id);
  DCHECK(render_frame_host->IsRenderFrameLive());

  auto target = content::ToRenderFrameHost(render_frame_host);
  return content::EvalJs(target, command,
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractBool();
}

// Returns whether the feature is in fact enabled, rather that was requested to
// be enabled.
bool SupervisedUserIframeFilterTest::IsLocalWebApprovalsEnabled() const {
  return supervised_user::IsLocalWebApprovalsEnabled();
}

INSTANTIATE_TEST_SUITE_P(
    LocalWebApprovalsEnabled,
    SupervisedUserIframeFilterTest,
    testing::Combine(
        supervised_user::testing::LocalWebApprovalsTestCase::Values(),
        ThrottleTestParam::Values()),
    [](const testing::TestParamInfo<
        std::tuple<supervised_user::testing::LocalWebApprovalsTestCase,
                   ThrottleTestParam::FeatureStatus>>& info) {
      return base::StrCat(
          {"WithLocalWebApprovalsTestCase",
           static_cast<std::string>(std::get<0>(info.param)), "WithThrottle",
           ThrottleTestParam::ToString(std::get<1>(info.param))});
    });

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest, BlockSubFrame) {
  base::HistogramTester histogram_tester;

  BlockHost(kIframeHost2);
  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  // The first iframe's source is |kIframeHost1| and it is not blocked. It will
  // successfully load.
  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);

  content::FrameTreeNodeId blocked_frame_id = blocked[0];

  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id));

  permission_creator()->SetPermissionResult(true);
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame_id);
  EXPECT_EQ(permission_creator()->url_requests().size(), 1u);
  std::string requested_host = permission_creator()->url_requests()[0].host();

  EXPECT_EQ(requested_host, kIframeHost2);

  WaitForNavigationFinished(blocked[0],
                            permission_creator()->url_requests()[0]);

  EXPECT_FALSE(IsInterstitialBeingShownInFrame(blocked_frame_id));

  histogram_tester.ExpectUniqueSample(
      supervised_user::SupervisedUserInterstitial::
          kInterstitialCommandHistogramName,
      supervised_user::SupervisedUserInterstitial::Commands::
          REMOTE_ACCESS_REQUEST,
      1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::SupervisedUserInterstitial::
          kInterstitialPermissionSourceHistogramName,
      supervised_user::SupervisedUserInterstitial::RequestPermissionSource::
          SUB_FRAME,
      1);
}

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest, BlockMultipleSubFrames) {
  BlockHost(kIframeHost1);
  BlockHost(kIframeHost2);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 2u);

  content::FrameTreeNodeId blocked_frame_id_1 = blocked[0];
  GURL blocked_frame_url_1 = GetBlockedFrameURL(blocked_frame_id_1);

  content::FrameTreeNodeId blocked_frame_id_2 = blocked[1];
  GURL blocked_frame_url_2 = GetBlockedFrameURL(blocked_frame_id_2);

  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id_1));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame_id_1);
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame_id_2);

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

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest, TestBackButton) {
  BlockHost(kIframeHost1);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);

  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  SendCommandToFrame(kRemoteUrlAccessCommand, blocked[0]);

  std::string command = "document.getElementById('back-button').hidden;";

  auto* render_frame_host = tracker()->GetHost(blocked[0]);
  DCHECK(render_frame_host->IsRenderFrameLive());
  auto target = content::ToRenderFrameHost(render_frame_host);
  // Back button should be hidden in iframes.
  EXPECT_EQ(true, content::EvalJs(target, command,
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest,
                       TestBackButtonMainFrame) {
  BlockHost(kExampleHost);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);

  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  SendCommandToFrame(kRemoteUrlAccessCommand, blocked[0]);

  std::string command = "document.getElementById('back-button').hidden;";
  auto* render_frame_host = tracker()->GetHost(blocked[0]);
  DCHECK(render_frame_host->IsRenderFrameLive());

  auto target = content::ToRenderFrameHost(render_frame_host);
  bool value =
      content::EvalJs(target, command, content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool();

  // Back button should be hidden only when local web approvals is enabled due
  // to new UI for local web approvals.
  EXPECT_EQ(value, IsLocalWebApprovalsEnabled());
}

// Tests that the trivial www-subdomain stripping is applied on the url
// of the interstitial. Blocked urls without further conflicts will be
// unblocked by a remote approval.
// TODO(crbug.com/356676072): flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_BlockedMainFrameFromClassifyUrlForUnstripedHostIsStrippedInRemoteApproval \
  DISABLED_BlockedMainFrameFromClassifyUrlForUnstripedHostIsStrippedInRemoteApproval
#else
#define MAYBE_BlockedMainFrameFromClassifyUrlForUnstripedHostIsStrippedInRemoteApproval \
  BlockedMainFrameFromClassifyUrlForUnstripedHostIsStrippedInRemoteApproval
#endif
IN_PROC_BROWSER_TEST_P(
    SupervisedUserIframeFilterTest,
    MAYBE_BlockedMainFrameFromClassifyUrlForUnstripedHostIsStrippedInRemoteApproval) {
  // Classify url blocks the navigation to the target url.
  // No matching blocklist entry exists for the host of the target url.
  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);
  content::FrameTreeNodeId blocked_frame_id = blocked[0];
  GURL blocked_frame_url = GetBlockedFrameURL(blocked_frame_id);
  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id));

  // Request remote approval.
  permission_creator()->SetPermissionResult(true);
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame_id);
  EXPECT_EQ(permission_creator()->url_requests().size(), 1u);
  std::string requested_host = permission_creator()->url_requests()[0].host();

  // The trivial "www" subdomain is stripped for the url in the remote approval
  // request.
  EXPECT_EQ(requested_host, kStrippedExampleHost);
  WaitForNavigationFinished(blocked_frame_id, blocked_url);
  // The unstriped url gets unblocked.
  EXPECT_FALSE(IsInterstitialBeingShownInFrame(blocked_frame_id));
}

// Tests that the url stripping is applied on the url on the interstitial, when
// there is no unstriped host entry in the blocklist.
IN_PROC_BROWSER_TEST_P(
    SupervisedUserIframeFilterTest,
    BlockedMainFrameFromBlockListIsStrippedInRemoteApproval) {
  // Manual parental blocklist entry blocks the navigation to the target url.
  BlockHost("*.example.*");
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);
  content::FrameTreeNodeId blocked_frame_id = blocked[0];
  GURL blocked_frame_url = GetBlockedFrameURL(blocked_frame_id);
  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id));

  // Request remote approval.
  permission_creator()->SetPermissionResult(true);
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame_id);
  EXPECT_EQ(permission_creator()->url_requests().size(), 1u);
  std::string requested_host = permission_creator()->url_requests()[0].host();

  // The trivial "www" subdomain has been stripped from the host in the
  // interstitial, because the conflicting entry in the blocklist is not a
  // www-subdomain conflict.
  EXPECT_EQ(requested_host, kStrippedExampleHost);
}

// Tests that the url stripping is skipped on the url on the interstitial, when
// there is a unstriped host entry in the blocklist. Blocked urls without
// further conflicts will be unblocked by a remote approval.
IN_PROC_BROWSER_TEST_P(
    SupervisedUserIframeFilterTest,
    BlockedMainFrameFromBlockListForUnstripedHostSkipsStrippingInRemoteApproval) {
  // Manual parental blocklist entry for the unstriped url blocks the
  // navigation to the target url.
  BlockHost(kExampleHost);
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);
  content::FrameTreeNodeId blocked_frame_id = blocked[0];
  GURL blocked_frame_url = GetBlockedFrameURL(blocked_frame_id);
  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame_id));

  // Request remote approval.
  permission_creator()->SetPermissionResult(true);
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame_id);
  EXPECT_EQ(permission_creator()->url_requests().size(), 1u);
  std::string requested_host = permission_creator()->url_requests()[0].host();

  // The stripping has been skipped for the url of the interstitial, because an
  // identical entry exists in the blocklist. The interstitial contains the full
  // url.
  EXPECT_EQ(requested_host, kExampleHost);
  // The navigation gets unblocked.
  WaitForNavigationFinished(blocked_frame_id, blocked_frame_url);
  EXPECT_FALSE(IsInterstitialBeingShownInFrame(blocked_frame_id));
}

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest,
                       AllowlistedMainFrameDenylistedIframe) {
  AllowlistHost(kExampleHost);
  BlockHost(kIframeHost1);

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
  auto blocked = GetBlockedFrames();
  EXPECT_EQ(blocked.size(), 1u);
  EXPECT_EQ(kIframeHost1, GetBlockedFrameURL(blocked[0]).host());
}

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest,
                       RememberAlreadyRequestedHosts) {
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked_frames = GetBlockedFrames();
  EXPECT_EQ(blocked_frames.size(), 1u);

  // Expect that remote approvals button is shown.
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frames[0]));
  // Expect that the local approvals button is shown if the flag is enabled.
  EXPECT_EQ(IsLocalWebApprovalsEnabled(),
            IsLocalApprovalsButtonBeingShown(blocked_frames[0]));
  // Expect that the "Block reason" is shown.
  EXPECT_TRUE(IsBlockReasonBeingShown(blocked_frames[0]));

  // Delay approval/denial by parent.
  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  // Request permission.
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frames[0]);

  // Navigate to another allowed url.
  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/with_iframes.html");
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  // Navigate back to the blocked url.
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  // Error page is being shown, but "Ask Permission" button is not being shown.
  EXPECT_FALSE(IsRemoteApprovalsButtonBeingShown(blocked_frames[0]));
  // Expect that the local approvals instead button is shown on the page if the
  // flag is enabled.
  EXPECT_EQ(IsLocalWebApprovalsEnabled(),
            IsLocalApprovalsInsteadButtonBeingShown(blocked_frames[0]));
  // Expect that the "Block reason" is not shown.
  EXPECT_FALSE(IsBlockReasonBeingShown(blocked_frames[0]));

  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(active_contents);
  ASSERT_NE(navigation_observer, nullptr);

  EXPECT_TRUE(base::Contains(navigation_observer->requested_hosts_for_test(),
                             kExampleHost));

  NavigationFinishedWaiter waiter(
      active_contents,
      active_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      blocked_url);
  permission_creator()->HandleDelayedRequests();
  waiter.Wait();

  EXPECT_FALSE(base::Contains(navigation_observer->requested_hosts_for_test(),
                              kExampleHost));

  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

IN_PROC_BROWSER_TEST_P(SupervisedUserIframeFilterTest,
                       IFramesWithSameDomainAsMainFrameAllowed) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  supervised_user::SupervisedUserURLFilter* filter = service->GetURLFilter();

  // Set the default behavior to block.
  filter->SetDefaultFilteringBehavior(
      supervised_user::FilteringBehavior::kBlock);

  base::RunLoop().RunUntilIdle();

  // Allows |www.example.com|.
  AllowlistHost("*.example.*");

  // |with_frames_same_domain.html| contains subframes with "a.example.com" and
  // "b.example.com", and "c.example2.com" urls.
  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes_same_domain.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked_frames = GetBlockedFrames();
  EXPECT_EQ(blocked_frames.size(), 1u);
  EXPECT_EQ(GetBlockedFrameURL(blocked_frames[0]).host(), "www.c.example2.com");
}

// The switches::kHostWindowBounds commandline flag doesn't appear to work
// for tests on other platforms.
// TODO(b/300426225): enable these tests on Linux/Mac/Windows.
#if BUILDFLAG(IS_CHROMEOS_ASH)
class SupervisedUserNarrowWidthIframeFilterTest
    : public SupervisedUserIframeFilterTest {
 protected:
  SupervisedUserNarrowWidthIframeFilterTest() = default;
  ~SupervisedUserNarrowWidthIframeFilterTest() override = default;

  void SetUp() override;
};

void SupervisedUserNarrowWidthIframeFilterTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kHostWindowBounds, "0+0-400x800");
  SupervisedUserIframeFilterTest::SetUp();
}

INSTANTIATE_TEST_SUITE_P(
    LocalWebApprovalsEnabledNarrowWidth,
    SupervisedUserNarrowWidthIframeFilterTest,
    testing::Combine(
        supervised_user::testing::LocalWebApprovalsTestCase::OnlySupported(),
        ThrottleTestParam::Values()),
    [](const testing::TestParamInfo<
        std::tuple<supervised_user::testing::LocalWebApprovalsTestCase,
                   ThrottleTestParam::FeatureStatus>>& info) {
      return base::StrCat(
          {"WithLocalWebApprovalsTestCase",
           static_cast<std::string>(std::get<0>(info.param)), "WithThrottle",
           ThrottleTestParam::ToString(std::get<1>(info.param))});
    });

IN_PROC_BROWSER_TEST_P(SupervisedUserNarrowWidthIframeFilterTest,
                       NarrowWidthWindow) {
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  auto blocked_frames = GetBlockedFrames();
  EXPECT_EQ(blocked_frames.size(), 1u);

  // Expect that remote approvals button is shown.
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frames[0]));
  // Expect that the local approvals button is shown if the flag is enabled.
  EXPECT_EQ(IsLocalWebApprovalsEnabled(),
            IsLocalApprovalsButtonBeingShown(blocked_frames[0]));
  // Expect that the "Details" link is shown.
  EXPECT_TRUE(IsDetailsLinkBeingShown(blocked_frames[0]));

  // Delay approval/denial by parent.
  permission_creator()->SetPermissionResult(true);
  permission_creator()->DelayHandlingForNextRequests();

  // Request permission.
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frames[0]);

  // Navigate to another allowed url.
  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  // Navigate back to the blocked url.
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  // Error page is being shown, but "Ask Permission" button is not being shown.
  EXPECT_FALSE(IsRemoteApprovalsButtonBeingShown(blocked_frames[0]));
  // Expect that the local approvals instead button is shown on the page if the
  // flag is enabled.
  EXPECT_EQ(IsLocalWebApprovalsEnabled(),
            IsLocalApprovalsInsteadButtonBeingShown(blocked_frames[0]));
  // "Details" link is not shown.
  EXPECT_FALSE(IsDetailsLinkBeingShown(blocked_frames[0]));

  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(active_contents);
  ASSERT_NE(navigation_observer, nullptr);

  EXPECT_TRUE(base::Contains(navigation_observer->requested_hosts_for_test(),
                             kExampleHost));

  NavigationFinishedWaiter waiter(
      active_contents,
      active_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      blocked_url);
  permission_creator()->HandleDelayedRequests();
  waiter.Wait();

  EXPECT_FALSE(base::Contains(navigation_observer->requested_hosts_for_test(),
                              kExampleHost));

  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

// Tests Chrome OS local web approvals flow.
using ChromeOSLocalWebApprovalsTest = SupervisedUserIframeFilterTest;

// Only test for the local web approvals feature enabled.
INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeOSLocalWebApprovalsTest,
    testing::Combine(
        supervised_user::testing::LocalWebApprovalsTestCase::OnlySupported(),
        ThrottleTestParam::Values()),
    [](const testing::TestParamInfo<
        std::tuple<supervised_user::testing::LocalWebApprovalsTestCase,
                   ThrottleTestParam::FeatureStatus>>& info) {
      return base::StrCat(
          {"WithLocalWebApprovalsTestCase",
           static_cast<std::string>(std::get<0>(info.param)), "WithThrottle",
           ThrottleTestParam::ToString(std::get<1>(info.param))});
    });

IN_PROC_BROWSER_TEST_P(ChromeOSLocalWebApprovalsTest,
                       StartLocalWebApprovalsFromMainFrame) {
  base::HistogramTester histogram_tester;
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  const std::vector<content::FrameTreeNodeId> blocked_frames =
      GetBlockedFrames();
  ASSERT_EQ(blocked_frames.size(), 1u);
  const content::FrameTreeNodeId blocked_frame = blocked_frames[0];
  EXPECT_TRUE(IsLocalApprovalsButtonBeingShown(blocked_frame));
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frame));
  CheckPreferredApprovalButton(blocked_frame);

  // Trigger local approval flow - native dialog should appear.
  SendCommandToFrame(kLocalUrlAccessCommand, blocked_frame);
  EXPECT_TRUE(ash::ParentAccessDialog::GetInstance());

  // Close the flow without approval - interstitial should be still shown.
  ui::test::EventGenerator generator(ash::Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_FALSE(ash::ParentAccessDialog::GetInstance());
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));
  EXPECT_TRUE(IsLocalApprovalsButtonBeingShown(blocked_frame));
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frame));
  CheckPreferredApprovalButton(blocked_frame);

  histogram_tester.ExpectUniqueSample(
      supervised_user::SupervisedUserInterstitial::
          kInterstitialCommandHistogramName,
      supervised_user::SupervisedUserInterstitial::Commands::
          LOCAL_ACCESS_REQUEST,
      1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::SupervisedUserInterstitial::
          kInterstitialPermissionSourceHistogramName,
      supervised_user::SupervisedUserInterstitial::RequestPermissionSource::
          MAIN_FRAME,
      1);
}

IN_PROC_BROWSER_TEST_P(ChromeOSLocalWebApprovalsTest,
                       StartLocalWebApprovalsFromIframe) {
  base::HistogramTester histogram_tester;
  BlockHost(kIframeHost1);

  const GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));

  const std::vector<content::FrameTreeNodeId> blocked_frames =
      GetBlockedFrames();
  ASSERT_EQ(blocked_frames.size(), 1u);
  const content::FrameTreeNodeId blocked_frame = blocked_frames[0];
  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame));
  EXPECT_TRUE(IsLocalApprovalsButtonBeingShown(blocked_frame));
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frame));
  CheckPreferredApprovalButton(blocked_frame);

  // Trigger local approval flow - native dialog should appear.
  SendCommandToFrame(kLocalUrlAccessCommand, blocked_frame);
  EXPECT_TRUE(ash::ParentAccessDialog::GetInstance());

  // Close the flow without approval - interstitial should be still shown.
  ui::test::EventGenerator generator(ash::Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_FALSE(ash::ParentAccessDialog::GetInstance());
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
  EXPECT_TRUE(IsInterstitialBeingShownInFrame(blocked_frame));
  EXPECT_TRUE(IsLocalApprovalsButtonBeingShown(blocked_frame));
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frame));
  CheckPreferredApprovalButton(blocked_frame);

  histogram_tester.ExpectUniqueSample(
      supervised_user::SupervisedUserInterstitial::
          kInterstitialCommandHistogramName,
      supervised_user::SupervisedUserInterstitial::Commands::
          LOCAL_ACCESS_REQUEST,
      1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::SupervisedUserInterstitial::
          kInterstitialPermissionSourceHistogramName,
      supervised_user::SupervisedUserInterstitial::RequestPermissionSource::
          SUB_FRAME,
      1);
}

IN_PROC_BROWSER_TEST_P(ChromeOSLocalWebApprovalsTest,
                       UpdateUIAfterRemoteRequestSent) {
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  const std::vector<content::FrameTreeNodeId> blocked_frames =
      GetBlockedFrames();
  ASSERT_EQ(blocked_frames.size(), 1u);
  const content::FrameTreeNodeId blocked_frame = blocked_frames[0];
  EXPECT_TRUE(IsLocalApprovalsButtonBeingShown(blocked_frame));
  EXPECT_TRUE(IsRemoteApprovalsButtonBeingShown(blocked_frame));

  // Trigger remote approval flow - ui should change.
  SendCommandToFrame(kRemoteUrlAccessCommand, blocked_frame);

  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));
  EXPECT_FALSE(IsLocalApprovalsButtonBeingShown(blocked_frame));
  EXPECT_FALSE(IsRemoteApprovalsButtonBeingShown(blocked_frame));
  EXPECT_TRUE(IsLocalApprovalsInsteadButtonBeingShown(blocked_frame));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<
          std::tuple<supervised_user::SupervisionMixin::SignInMode,
                     ThrottleTestParam::FeatureStatus>> {
 protected:
  static supervised_user::SupervisionMixin::SignInMode GetSignInMode() {
    return std::get<0>(GetParam());
  }
  static ThrottleTestParam::FeatureStatus GetThrottleStatus() {
    return std::get<1>(GetParam());
  }
  SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers()
      : SupervisedUserNavigationThrottleTestBase(GetSignInMode()) {
    ThrottleTestParam::InitFeatureList(scoped_feature_list_,
                                       GetThrottleStatus());
  }
  ~SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers,
    testing::Combine(
        testing::Values(
            supervised_user::SupervisionMixin::SignInMode::kRegular,
            supervised_user::SupervisionMixin::SignInMode::kSupervised),
        ThrottleTestParam::Values()),
    [](const testing::TestParamInfo<
        std::tuple<supervised_user::SupervisionMixin::SignInMode,
                   ThrottleTestParam::FeatureStatus>>& info) {
      return base::StrCat(
          {"WithSignInMode", SignInModeAsString(std::get<0>(info.param)),
           "WithThrottle",
           ThrottleTestParam::ToString(std::get<1>(info.param))});
    });

IN_PROC_BROWSER_TEST_P(
    SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers,
    CheckAgainstBlocklist) {
  BlockHost(kExampleHost2);

  // Classify url is never called - if ever, a static blocklist should be used.
  EXPECT_CALL(kids_management_api_mock().classify_url_mock(),
              ClassifyUrl(testing::_))
      .Times(0);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));

  // Only supervised users should experience the interstitial
  EXPECT_EQ(IsInterstitialBeingShownInMainFrame(browser()),
            GetSignInMode() ==
                supervised_user::SupervisionMixin::SignInMode::kSupervised);
}

// TODO(crbug.com/359268574): Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CheckAgainstClassifyUrlRPC DISABLED_CheckAgainstClassifyUrlRPC
#else
#define MAYBE_CheckAgainstClassifyUrlRPC CheckAgainstClassifyUrlRPC
#endif
IN_PROC_BROWSER_TEST_P(
    SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers,
    MAYBE_CheckAgainstClassifyUrlRPC) {
  kids_management_api_mock().RestrictSubsequentClassifyUrl();

  // Only supervised users should call the backend
  if (GetSignInMode() ==
      supervised_user::SupervisionMixin::SignInMode::kSupervised) {
    // TODO(b/358283493): Explain why throttles check the URL twice.
    EXPECT_CALL(kids_management_api_mock().classify_url_mock(),
                ClassifyUrl(testing::_))
        .Times(::testing::AtLeast(1));
  } else {
    EXPECT_CALL(kids_management_api_mock().classify_url_mock(),
                ClassifyUrl(testing::_))
        .Times(0);
  }

  GURL url = embedded_test_server()->GetURL(kExampleHost2,
                                            "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Only supervised users should experience the
  // interstitial
  EXPECT_EQ(IsInterstitialBeingShownInMainFrame(browser()),
            GetSignInMode() ==
                supervised_user::SupervisionMixin::SignInMode::kSupervised);
}

// Flaky: crbug.com/361463503
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CheckSameDocumentNavigationAgainstClassifyUrlRPC \
  DISABLED_CheckSameDocumentNavigationAgainstClassifyUrlRPC
#else
#define MAYBE_CheckSameDocumentNavigationAgainstClassifyUrlRPC \
  CheckSameDocumentNavigationAgainstClassifyUrlRPC
#endif  // BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(
    SupervisedUserNavigationThrottleOnlyEnabledForSupervisedUsers,
    MAYBE_CheckSameDocumentNavigationAgainstClassifyUrlRPC) {
  kids_management_api_mock().RestrictSubsequentClassifyUrl();

  // Only supervised users should call the backend
  if (GetSignInMode() ==
      supervised_user::SupervisionMixin::SignInMode::kSupervised) {
    // TODO(b/358283493): Explain why throttles check the URL twice.
    EXPECT_CALL(kids_management_api_mock().classify_url_mock(),
                ClassifyUrl(testing::_))
        .Times(::testing::AtLeast(1));
  } else {
    EXPECT_CALL(kids_management_api_mock().classify_url_mock(),
                ClassifyUrl(testing::_))
        .Times(0);
  }

  GURL url = embedded_test_server()->GetURL(kExampleHost2,
                                            "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Only supervised users should experience the
  // interstitial
  EXPECT_EQ(IsInterstitialBeingShownInMainFrame(browser()),
            GetSignInMode() ==
                supervised_user::SupervisionMixin::SignInMode::kSupervised);

  // Simulate a same document navigation by navigating to #test.
  GURL::Replacements replace_ref;
  replace_ref.SetRefStr("test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           url.ReplaceComponents(replace_ref)));

  // Only supervised users should experience the
  // interstitial
  EXPECT_EQ(IsInterstitialBeingShownInMainFrame(browser()),
            GetSignInMode() ==
                supervised_user::SupervisionMixin::SignInMode::kSupervised);
}

class SupervisedUserNavigationThrottleFencedFramesTest
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<ThrottleTestParam::FeatureStatus> {
 public:
  SupervisedUserNavigationThrottleFencedFramesTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {
    ThrottleTestParam::InitFeatureList(scoped_feature_list_, GetParam());
  }
  ~SupervisedUserNavigationThrottleFencedFramesTest() override = default;
  SupervisedUserNavigationThrottleFencedFramesTest(
      const SupervisedUserNavigationThrottleFencedFramesTest&) = delete;

  SupervisedUserNavigationThrottleFencedFramesTest& operator=(
      const SupervisedUserNavigationThrottleFencedFramesTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationThrottleFencedFramesTest,
    ThrottleTestParam::Values(),
    [](const testing::TestParamInfo<ThrottleTestParam::FeatureStatus>& info) {
      return base::StrCat(
          {"WithThrottle", ThrottleTestParam::ToString(info.param)});
    });

IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationThrottleFencedFramesTest,
                       BlockFencedFrame) {
  BlockHost(kIframeHost2);
  const GURL kInitialUrl = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");

  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  // Same origin fenced frame is not blocked, and therefore must be allowed.
  const GURL kSameOriginFencedFrameUrl = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/fenced_frame.html");
  content::RenderFrameHost* rfh_same_origin =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), kSameOriginFencedFrameUrl);
  EXPECT_TRUE(rfh_same_origin);

  // Host1 is not blocked, and therefore must be allowed.
  const GURL kHost1FencedFrameUrl = embedded_test_server()->GetURL(
      kIframeHost1, "/supervised_user/fenced_frame.html");
  content::RenderFrameHost* rfh_host1 =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), kHost1FencedFrameUrl);
  EXPECT_TRUE(rfh_host1);

  // Host2 is blocked, and therefore should result in a interstitial being
  // shown, which is validated by the expectation of net::Error::ERR_FAILED.
  const GURL kHost2FencedFrameUrl = embedded_test_server()->GetURL(
      kIframeHost2, "/supervised_user/fenced_frame.html");
  content::RenderFrameHost* rfh_host2 =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), kHost2FencedFrameUrl,
          net::Error::ERR_FAILED);
  EXPECT_TRUE(rfh_host2);
}

}  // namespace

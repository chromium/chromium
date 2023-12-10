// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
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
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/features_testutils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/core/common/supervised_user_utils.h"
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

using content::NavigationController;
using content::WebContents;

namespace {

static const char* kExampleHost = "www.example.com";
static const char* kExampleHost2 = "www.example2.com";
static const char* kFamiliesHost = "families.google.com";
static const char* kIframeHost1 = "www.iframe1.com";
static const char* kIframeHost2 = "www.iframe2.com";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kLocalUrlAccessCommand[] = "requestUrlAccessLocal";
#endif
constexpr char kRemoteUrlAccessCommand[] = "requestUrlAccessRemote";

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
    if (!base::Contains(render_frame_hosts_, frame_id)) {
      return nullptr;
    }
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
  if (!base::Contains(render_frame_hosts_, frame_tree_node_id)) {
    return;
  }

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
  if (web_contents()->GetInnerWebContents().size() > 0u) {
    return;
  }
  run_loop_.Run();
}

// Helper class to wait for a particular navigation in a particular render
// frame in tests.
class NavigationFinishedWaiter : public content::WebContentsObserver {
 public:
  NavigationFinishedWaiter(content::WebContents* web_contents,
                           int frame_id,
                           const GURL& url);
  NavigationFinishedWaiter(const NavigationFinishedWaiter&) = delete;
  NavigationFinishedWaiter& operator=(const NavigationFinishedWaiter&) = delete;
  ~NavigationFinishedWaiter() override = default;

  void Wait();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  int frame_id_;
  GURL url_;
  bool did_finish_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

NavigationFinishedWaiter::NavigationFinishedWaiter(
    content::WebContents* web_contents,
    int frame_id,
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
}  // namespace

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
                                        "*.families.google.com, "
                                        "*.iframe1.com, *.iframe2.com"},
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
                                                      /* allowlist */ false);
  }

  void AllowlistHost(const std::string& host) {
    supervised_user_test_util::SetManualFilterForHost(
        browser()->profile(), host, /* allowlist */ true);
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list_{
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS};
#endif
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
      public testing::WithParamInterface<
          supervised_user::testing::EnableProtoApiForClassifyUrlTestCase> {
 protected:
  static supervised_user::testing::EnableProtoApiForClassifyUrlTestCase
  GetEnableProtoApiTestParam() {
    return GetParam();
  }

  SupervisedUserNavigationThrottleTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {}
  ~SupervisedUserNavigationThrottleTest() override = default;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> enable_proto_api_feature_{
      GetEnableProtoApiTestParam().MakeFeatureList()};
};

class SupervisedUserNavigationThrottleWithPrerenderingTest
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<std::tuple<
          std::string,
          supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>> {
 protected:
  SupervisedUserNavigationThrottleWithPrerenderingTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {}
  ~SupervisedUserNavigationThrottleWithPrerenderingTest() override = default;

  static std::string GetTargetHint() { return std::get<0>(GetParam()); }
  static supervised_user::testing::EnableProtoApiForClassifyUrlTestCase
  GetEnableProtoApiTestParam() {
    return std::get<1>(GetParam());
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> enable_proto_api_feature_{
      GetEnableProtoApiTestParam().MakeFeatureList()};
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationThrottleWithPrerenderingTest,
    testing::Combine(testing::Values("_self", "_blank"),
                     supervised_user::testing::
                         EnableProtoApiForClassifyUrlTestCase::Values()),
    [](const testing::TestParamInfo<std::tuple<
           std::string,
           supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>>&
           info) {
      return std::get<0>(info.param) +
             static_cast<std::string>(std::get<1>(info.param));
    });

// Tests that prerendering fails in supervised user mode.
#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/1259146): Flaky on ChromeOS.
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
  int host_id = host_creation_waiter.Wait();
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
IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationThrottleTest,
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

IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationThrottleTest,
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

IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationThrottleTest,
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

IN_PROC_BROWSER_TEST_P(SupervisedUserNavigationThrottleTest,
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

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationThrottleTest,
    supervised_user::testing::EnableProtoApiForClassifyUrlTestCase::Values(),
    [](const testing::TestParamInfo<
        supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>& info) {
      return static_cast<std::string>(info.param);
    });

class SupervisedUserIframeFilterTest
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<std::tuple<
          supervised_user::testing::LocalWebApprovalsTestCase,
          supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>> {
 protected:
  SupervisedUserIframeFilterTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {}

  ~SupervisedUserIframeFilterTest() override = default;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  std::vector<int> GetBlockedFrames();
  const GURL& GetBlockedFrameURL(int frame_id);
  bool IsInterstitialBeingShownInFrame(int frame_id);
  bool IsRemoteApprovalsButtonBeingShown(int frame_id);
  bool IsLocalApprovalsButtonBeingShown(int frame_id);
  bool IsBlockReasonBeingShown(int frame_id);
  bool IsDetailsLinkBeingShown(int frame_id);
  void CheckPreferredApprovalButton(int frame_id);
  bool IsLocalApprovalsInsteadButtonBeingShown(int frame_id);
  void SendCommandToFrame(const std::string& command_name, int frame_id);
  void WaitForNavigationFinished(int frame_id, const GURL& url);
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
  static supervised_user::testing::EnableProtoApiForClassifyUrlTestCase
  GetEnableProtoApiTestParam() {
    return std::get<1>(GetParam());
  }

 private:
  bool RunCommandAndGetBooleanFromFrame(int frame_id,
                                        const std::string& command);

  std::unique_ptr<RenderFrameTracker> tracker_;
  raw_ptr<supervised_user::PermissionRequestCreatorMock,
          DanglingUntriaged | ExperimentalAsh>
      permission_creator_;

  // Each feature is enabled within its own feature list.
  std::unique_ptr<base::test::ScopedFeatureList>
      local_web_approvals_test_support_{
          GetLocalWebApprovalsSupportTestParam().MakeFeatureList()};
  std::unique_ptr<base::test::ScopedFeatureList> enable_proto_api_feature_{
      GetEnableProtoApiTestParam().MakeFeatureList()};
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

std::vector<int> SupervisedUserIframeFilterTest::GetBlockedFrames() {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(tab);
  const auto& interstitials = navigation_observer->interstitials_for_test();

  std::vector<int> blocked_frames;
  blocked_frames.reserve(interstitials.size());

  for (const auto& elem : interstitials) {
    blocked_frames.push_back(elem.first);
  }

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
      "document.getElementsByClassName('supervised-user-block') != null";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsBlockReasonBeingShown(int frame_id) {
  std::string command =
      "getComputedStyle(document.getElementById('block-reason')).display !== "
      "\"none\"";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsDetailsLinkBeingShown(int frame_id) {
  std::string command =
      "getComputedStyle(document.getElementById('block-reason-show-details-"
      "link')).display !== \"none\"";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsRemoteApprovalsButtonBeingShown(
    int frame_id) {
  std::string command =
      "!document.getElementById('remote-approvals-button').hidden";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

bool SupervisedUserIframeFilterTest::IsLocalApprovalsButtonBeingShown(
    int frame_id) {
  std::string command =
      "!document.getElementById('local-approvals-button').hidden";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

void SupervisedUserIframeFilterTest::CheckPreferredApprovalButton(
    int frame_id) {
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
    int frame_id) {
  std::string command =
      "!document.getElementById('local-approvals-remote-request-sent-button')."
      "hidden";
  return RunCommandAndGetBooleanFromFrame(frame_id, command);
}

void SupervisedUserIframeFilterTest::SendCommandToFrame(
    const std::string& command_name,
    int frame_id) {
  auto* render_frame_host = tracker()->GetHost(frame_id);
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsRenderFrameLive());
  std::string command = base::StrCat({"sendCommand(\'", command_name, "\')"});
  ASSERT_TRUE(
      content::ExecJs(content::ToRenderFrameHost(render_frame_host), command));
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
        supervised_user::testing::EnableProtoApiForClassifyUrlTestCase::
            Values()),
    [](const testing::TestParamInfo<std::tuple<
           supervised_user::testing::LocalWebApprovalsTestCase,
           supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>>&
           info) {
      return static_cast<std::string>(std::get<0>(info.param)) +
             static_cast<std::string>(std::get<1>(info.param));
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

  int blocked_frame_id = blocked[0];

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

  int blocked_frame_id_1 = blocked[0];
  GURL blocked_frame_url_1 = GetBlockedFrameURL(blocked_frame_id_1);

  int blocked_frame_id_2 = blocked[1];
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
                       IframesWithSameDomainAsMainFrameAllowed) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  supervised_user::SupervisedUserURLFilter* filter = service->GetURLFilter();

  // Set the default behavior to block.
  filter->SetDefaultFilteringBehavior(
      supervised_user::FilteringBehavior::kBlock);

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
        supervised_user::testing::EnableProtoApiForClassifyUrlTestCase::
            Values()),
    [](const testing::TestParamInfo<std::tuple<
           supervised_user::testing::LocalWebApprovalsTestCase,
           supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>>&
           info) {
      return static_cast<std::string>(std::get<0>(info.param)) +
             static_cast<std::string>(std::get<1>(info.param));
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
        supervised_user::testing::EnableProtoApiForClassifyUrlTestCase::
            Values()),
    [](const testing::TestParamInfo<std::tuple<
           supervised_user::testing::LocalWebApprovalsTestCase,
           supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>>&
           info) {
      return static_cast<std::string>(std::get<0>(info.param)) +
             static_cast<std::string>(std::get<1>(info.param));
    });

IN_PROC_BROWSER_TEST_P(ChromeOSLocalWebApprovalsTest,
                       StartLocalWebApprovalsFromMainFrame) {
  base::HistogramTester histogram_tester;
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  EXPECT_TRUE(IsInterstitialBeingShownInMainFrame(browser()));

  const std::vector<int> blocked_frames = GetBlockedFrames();
  ASSERT_EQ(blocked_frames.size(), 1u);
  const int blocked_frame = blocked_frames[0];
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

  const std::vector<int> blocked_frames = GetBlockedFrames();
  ASSERT_EQ(blocked_frames.size(), 1u);
  const int blocked_frame = blocked_frames[0];
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

  const std::vector<int> blocked_frames = GetBlockedFrames();
  ASSERT_EQ(blocked_frames.size(), 1u);
  const int blocked_frame = blocked_frames[0];
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

class SupervisedUserNavigationThrottleNotSupervisedTest
    : public SupervisedUserNavigationThrottleTestBase {
 protected:
  SupervisedUserNavigationThrottleNotSupervisedTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kRegular) {}
  ~SupervisedUserNavigationThrottleNotSupervisedTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserNavigationThrottleNotSupervisedTest,
                       DontBlock) {
  BlockHost(kExampleHost);

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  // Even though the URL is marked as blocked, the load should go through, since
  // the user isn't supervised.
  EXPECT_FALSE(IsInterstitialBeingShownInMainFrame(browser()));
}

class SupervisedUserNavigationThrottleFencedFramesTest
    : public SupervisedUserNavigationThrottleTestBase,
      public testing::WithParamInterface<
          supervised_user::testing::EnableProtoApiForClassifyUrlTestCase> {
 public:
  static supervised_user::testing::EnableProtoApiForClassifyUrlTestCase
  GetEnableProtoApiTestParam() {
    return GetParam();
  }

  SupervisedUserNavigationThrottleFencedFramesTest()
      : SupervisedUserNavigationThrottleTestBase(
            supervised_user::SupervisionMixin::SignInMode::kSupervised) {}
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
  std::unique_ptr<base::test::ScopedFeatureList> enable_proto_api_feature_{
      GetEnableProtoApiTestParam().MakeFeatureList()};
};

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

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserNavigationThrottleFencedFramesTest,

    supervised_user::testing::EnableProtoApiForClassifyUrlTestCase::Values(),
    [](const testing::TestParamInfo<
        supervised_user::testing::EnableProtoApiForClassifyUrlTestCase>& info) {
      return static_cast<std::string>(info.param);
    });

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_navigation_throttle.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "net/http/http_response_headers.h"
#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {
namespace {

using ::testing::Return;

class TestActorNavigationDelegate : public ActorNavigationThrottle::Delegate {
 public:
  TestActorNavigationDelegate() = default;
  ~TestActorNavigationDelegate() override = default;

  bool MaybeDeferNavigation(const GURL& url,
                            NavigationConfirmedCallback callback) override {
    confirm_navigation_called_ = true;
    if (should_defer_) {
      pending_callback_ = std::move(callback);
      return true;
    }
    return false;
  }

  void RespondToNavigation(bool proceed) {
    if (pending_callback_) {
      std::move(pending_callback_).Run(proceed);
    }
  }

  bool confirm_navigation_called() const { return confirm_navigation_called_; }
  void set_should_defer(bool should_defer) { should_defer_ = should_defer; }

  base::WeakPtr<TestActorNavigationDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool should_defer_ = false;
  bool confirm_navigation_called_ = false;
  base::OnceCallback<void(bool)> pending_callback_;
  base::WeakPtrFactory<TestActorNavigationDelegate> weak_factory_{this};
};

class ActorNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kGlicCrossOriginNavigationGating,
                              features::kGlicActor},
        /*disabled_features=*/{});
    ChromeRenderViewHostTestHarness::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ActorNavigationThrottleTest, PrerenderedMainFrame_CancelIfDeferred) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  NavigateAndCommit(GURL("https://source.com"));

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://destination.com"), main_rfh());
  ON_CALL(handle, IsInPrerenderedMainFrame()).WillByDefault(Return(true));
  handle.set_initiator_origin(
      url::Origin::Create(GURL("https://initiator.com")));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  // WillProcessResponse should return CANCEL_AND_IGNORE because the engine
  // would normally DEFER this cross-origin navigation, but it's a prerender.
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle.WillProcessResponse().action());
}

TEST_F(ActorNavigationThrottleTest, PrerenderedMainFrame_ProceedIfSameOrigin) {
  const GURL kPageUrl("https://example.com");
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  NavigateAndCommit(kPageUrl);

  testing::NiceMock<content::MockNavigationHandle> handle(kPageUrl, main_rfh());
  ON_CALL(handle, IsInPrerenderedMainFrame()).WillByDefault(Return(true));
  handle.set_initiator_origin(url::Origin::Create(kPageUrl));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillProcessResponse().action());
}

TEST_F(ActorNavigationThrottleTest, BrowserInitiated_DeferAndConfirm) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  NavigateAndCommit(GURL("https://source.com"));

  TestActorNavigationDelegate test_delegate;
  test_delegate.set_should_defer(true);
  task->SetNavigationDelegate(test_delegate.GetWeakPtr());

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://destination.com"), main_rfh());
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(::ui::PAGE_TRANSITION_HOME_PAGE);
  handle.set_initiator_origin(url::Origin::Create(GURL("https://source.com")));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle.WillStartRequest().action());
  EXPECT_TRUE(test_delegate.confirm_navigation_called());

  bool cancel_called = false;
  throttle.set_cancel_deferred_navigation_callback_for_testing(
      base::BindLambdaForTesting(
          [&](content::NavigationThrottle::ThrottleCheckResult result) {
            cancel_called = true;
            EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
                      result.action());
          }));

  test_delegate.RespondToNavigation(false);
  EXPECT_TRUE(cancel_called);
}

TEST_F(ActorNavigationThrottleTest, BrowserInitiated_DeferAndProceed) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  NavigateAndCommit(GURL("https://source.com"));

  TestActorNavigationDelegate test_delegate;
  test_delegate.set_should_defer(true);
  task->SetNavigationDelegate(test_delegate.GetWeakPtr());

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://destination.com"), main_rfh());
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(::ui::PAGE_TRANSITION_HOME_PAGE);
  handle.set_initiator_origin(url::Origin::Create(GURL("https://source.com")));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle.WillStartRequest().action());
  EXPECT_TRUE(test_delegate.confirm_navigation_called());

  bool resume_called = false;
  throttle.set_resume_callback_for_testing(
      base::BindLambdaForTesting([&]() { resume_called = true; }));

  test_delegate.RespondToNavigation(true);
  EXPECT_TRUE(resume_called);
  EXPECT_EQ(nullptr, service->GetTask(task_id));
}

TEST_F(ActorNavigationThrottleTest,
       BrowserInitiated_NoDeferForNonUiTransition) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  NavigateAndCommit(GURL("https://source.com"));

  TestActorNavigationDelegate test_delegate;
  test_delegate.set_should_defer(true);
  task->SetNavigationDelegate(test_delegate.GetWeakPtr());

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://destination.com"), main_rfh());
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(::ui::PAGE_TRANSITION_LINK);
  handle.set_initiator_origin(url::Origin::Create(GURL("https://source.com")));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillStartRequest().action());
  EXPECT_FALSE(test_delegate.confirm_navigation_called());
}

TEST_F(ActorNavigationThrottleTest, UserConfirmedLeave_Proceed) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  TestActorNavigationDelegate test_delegate;
  test_delegate.set_should_defer(true);
  task->SetNavigationDelegate(test_delegate.GetWeakPtr());

  NavigateAndCommit(GURL("https://site.com"));

  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://another-site.com"), main_rfh());
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(::ui::PAGE_TRANSITION_TYPED);
  handle.set_initiator_origin(url::Origin::Create(GURL("https://site.com")));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  // 1. First check should DEFER because it's a user UI navigation and delegate
  // says defer.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle.WillStartRequest().action());
  EXPECT_TRUE(test_delegate.confirm_navigation_called());

  bool resume_called = false;
  throttle.set_resume_callback_for_testing(
      base::BindLambdaForTesting([&]() { resume_called = true; }));

  // 2. Simulate user clicking "Leave".
  test_delegate.RespondToNavigation(true);
  EXPECT_TRUE(resume_called);

  // 3. Stop the task (removes it from active tasks list).
  service->StopTask(task_id, ActorTask::StoppedReason::kUserNavigatedAway);
  ASSERT_EQ(nullptr, service->GetTask(task_id));

  // 4. Verify that subsequent checks (like a redirect) PROCEED because of the
  // flag.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillRedirectRequest().action());
}

TEST_F(ActorNavigationThrottleTest, PlainAutoToplevel_Proceed) {
  ActorKeyedService* service = ActorKeyedService::Get(profile());
  TaskId task_id =
      service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_TRUE(task);

  NavigateAndCommit(GURL("https://site.com"));

  TestActorNavigationDelegate test_delegate;
  test_delegate.set_should_defer(true);
  task->SetNavigationDelegate(test_delegate.GetWeakPtr());

  // Plain AUTO_TOPLEVEL navigation (representing Glic navigation or non-UI
  // action)
  testing::NiceMock<content::MockNavigationHandle> handle(
      GURL("https://another-site.com"), main_rfh());
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(::ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  handle.set_initiator_origin(url::Origin::Create(GURL("https://site.com")));

  content::MockNavigationThrottleRegistry registry(&handle);
  ActorNavigationThrottle throttle =
      ActorNavigationThrottle::CreateForTesting(registry, *task);

  // Should PROCEED immediately without prompting the user!
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillStartRequest().action());
  EXPECT_FALSE(test_delegate.confirm_navigation_called());
}

class ActorNavigationThrottleMimeBypassTest
    : public ActorNavigationThrottleTest {
 protected:
  void RunMimeTest(const char* content_type_header,
                   content::NavigationThrottle::ThrottleAction expected_action) {
    ActorKeyedService* service = ActorKeyedService::Get(profile());
    TaskId task_id =
        service->CreateTask(TestTaskSourceInfo(), NoEnterprisePolicyChecker());
    ActorTask* task = service->GetTask(task_id);
    ASSERT_TRUE(task);

    NavigateAndCommit(GURL("https://example.com"));

    testing::NiceMock<content::MockNavigationHandle> handle(
        GURL("https://example.com/api"), main_rfh());
    handle.set_initiator_origin(url::Origin::Create(GURL("https://example.com")));

    if (content_type_header) {
      std::string raw_headers = base::StrCat(
          {"HTTP/1.1 200 OK\nContent-Type: ", content_type_header, "\n"});
      std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
      raw_headers += '\0';
      auto headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
      handle.set_response_headers(headers);
    } else {
      std::string raw_headers = "HTTP/1.1 200 OK\n";
      std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
      raw_headers += '\0';
      auto headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
      handle.set_response_headers(headers);
    }

    content::MockNavigationThrottleRegistry registry(&handle);
    ActorNavigationThrottle throttle =
        ActorNavigationThrottle::CreateForTesting(registry, *task);

    EXPECT_EQ(expected_action, throttle.WillProcessResponse().action());
  }
};

// --- Blocked MIME Types ---
// These tests verify that dangerous, raw tabular/code/structured data formats
// are successfully blocked (returning CANCEL_AND_IGNORE) to prevent actors
// from exfiltrating data displayed inside DOM-rendered pre tags.

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_ApplicationJson) {
  RunMimeTest("application/json",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_ApplicationLdJson) {
  RunMimeTest("application/ld+json",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_ApplicationXJavascript) {
  RunMimeTest("application/x-javascript",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_ApplicationHalJson) {
  RunMimeTest("application/hal+json",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_ApplicationXml) {
  RunMimeTest("application/xml",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_TextCsv) {
  RunMimeTest("text/csv", content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_TextCommaSeparatedValues) {
  RunMimeTest("text/comma-separated-values",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_TextTsv) {
  RunMimeTest("text/tsv", content::NavigationThrottle::CANCEL_AND_IGNORE);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Blocked_TextTabSeparatedValues) {
  RunMimeTest("text/tab-separated-values",
              content::NavigationThrottle::CANCEL_AND_IGNORE);
}

// --- Allowed MIME Types ---
// These tests verify that safe, fallback, or missing content types are allowed
// to proceed (returning PROCEED), guarding against accidentally "failing closed".

TEST_F(ActorNavigationThrottleMimeBypassTest, Allowed_TextPlain) {
  RunMimeTest("text/plain", content::NavigationThrottle::PROCEED);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Allowed_NoContentTypeHeader) {
  RunMimeTest(nullptr, content::NavigationThrottle::PROCEED);
}

TEST_F(ActorNavigationThrottleMimeBypassTest, Allowed_TextHtml) {
  RunMimeTest("text/html", content::NavigationThrottle::PROCEED);
}

}  // namespace
}  // namespace actor

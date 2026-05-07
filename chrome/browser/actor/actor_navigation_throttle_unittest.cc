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

}  // namespace
}  // namespace actor

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_navigation_throttle.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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
  TaskId task_id = service->CreateTask(NoEnterprisePolicyChecker());
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
  TaskId task_id = service->CreateTask(NoEnterprisePolicyChecker());
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

}  // namespace
}  // namespace actor

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_navigation_throttle.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_test_base.h"
#include "components/contextual_tasks/public/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace contextual_tasks {

class ContextualTasksNavigationThrottleTest
    : public ContextualTasksUiServiceTestBase {};

TEST_F(ContextualTasksNavigationThrottleTest,
       BrowserInitiated_UntrustedParamNotAppended) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kContextualTasks}, {});
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  content::MockNavigationHandle handle(ai_url,
                                       web_contents->GetPrimaryMainFrame());
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_renderer_initiated(false);
  handle.set_source_site_instance(nullptr);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  base::RunLoop run_loop;
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .WillOnce([&](const GURL& intercepted_url,
                    base::WeakPtr<tabs::TabInterface> tab, bool is_to_new_tab) {
        std::string value;
        EXPECT_FALSE(
            net::GetValueForKeyInQuery(intercepted_url, "cru", &value));
        run_loop.Quit();
      });

  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            throttle->WillStartRequest().action());
  run_loop.Run();
}

TEST_F(ContextualTasksNavigationThrottleTest,
       ProcessNavigation_AboutBlank_Proceeds) {
  GURL about_blank_url("about:blank");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  content::MockNavigationHandle handle(about_blank_url,
                                       web_contents->GetPrimaryMainFrame());
  handle.set_is_in_primary_main_frame(true);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(ContextualTasksNavigationThrottleTest,
       ProcessNavigation_DataUrl_Proceeds) {
  GURL data_url("data:text/html,test");
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  content::MockNavigationHandle handle(data_url,
                                       web_contents->GetPrimaryMainFrame());
  handle.set_is_in_primary_main_frame(true);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(ContextualTasksNavigationThrottleTest,
       ProcessNavigation_FeatureDisabled_Proceeds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kContextualTasks);
  GURL ai_url(kAiPageUrl);
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));

  content::MockNavigationHandle handle(ai_url,
                                       web_contents->GetPrimaryMainFrame());
  handle.set_is_in_primary_main_frame(true);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

}  // namespace contextual_tasks

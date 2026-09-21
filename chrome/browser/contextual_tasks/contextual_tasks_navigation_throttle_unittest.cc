// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_navigation_throttle.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_test_base.h"
#include "chrome/browser/contextual_tasks/guest_opener_user_data.h"
#include "components/contextual_tasks/public/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
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

TEST_F(ContextualTasksNavigationThrottleTest,
       ProcessNavigation_GuestOpener_RendererInitiated_Cancelled) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  GuestOpenerUserData::CreateForWebContents(web_contents.get());

  // Test with arbitrary URL.
  {
    content::MockNavigationHandle handle(
        GURL("https://attacker.example/persist.html"),
        web_contents->GetPrimaryMainFrame());
    handle.set_is_in_primary_main_frame(true);
    handle.set_is_renderer_initiated(true);

    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
        registry.throttles().back().get());

    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillStartRequest().action());
    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillRedirectRequest().action());
    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillCommitWithoutUrlLoader().action());
  }

  // Test with about:blank when renderer-initiated.
  {
    content::MockNavigationHandle handle(GURL("about:blank"),
                                         web_contents->GetPrimaryMainFrame());
    handle.set_is_in_primary_main_frame(true);
    handle.set_is_renderer_initiated(true);

    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
        registry.throttles().back().get());

    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillStartRequest().action());
    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillRedirectRequest().action());
    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillCommitWithoutUrlLoader().action());
  }

  // Test with data: URL when renderer-initiated.
  {
    content::MockNavigationHandle handle(GURL("data:text/html,test"),
                                         web_contents->GetPrimaryMainFrame());
    handle.set_is_in_primary_main_frame(true);
    handle.set_is_renderer_initiated(true);

    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

    ASSERT_EQ(1u, registry.throttles().size());
    auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
        registry.throttles().back().get());

    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillStartRequest().action());
    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillRedirectRequest().action());
    EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
              throttle->WillCommitWithoutUrlLoader().action());
  }
}

TEST_F(ContextualTasksNavigationThrottleTest,
       ProcessNavigation_GuestOpener_BrowserInitiated_AboutBlankProceeds) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  GuestOpenerUserData::CreateForWebContents(web_contents.get());

  content::MockNavigationHandle handle(GURL("about:blank"),
                                       web_contents->GetPrimaryMainFrame());
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_renderer_initiated(false);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillCommitWithoutUrlLoader().action());
}

TEST_F(ContextualTasksNavigationThrottleTest,
       ProcessNavigation_GuestOpener_BrowserInitiated_OtherUrlCancelled) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  GuestOpenerUserData::CreateForWebContents(web_contents.get());

  content::MockNavigationHandle handle(GURL("https://example.com"),
                                       web_contents->GetPrimaryMainFrame());
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_renderer_initiated(false);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest().action());
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillRedirectRequest().action());
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillCommitWithoutUrlLoader().action());
}

TEST_F(ContextualTasksNavigationThrottleTest,
       MaybeCreateAndAdd_GuestOpener_AddedEvenIfSubframe) {
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  GuestOpenerUserData::CreateForWebContents(web_contents.get());

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(web_contents->GetPrimaryMainFrame());
  rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* child_rfh = rfh_tester->AppendChild("subframe");
  content::MockNavigationHandle handle(GURL("https://attacker.example"),
                                       child_rfh);
  handle.set_is_renderer_initiated(true);
  ASSERT_FALSE(handle.IsInOutermostMainFrame());

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<ContextualTasksNavigationThrottle*>(
      registry.throttles().back().get());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest().action());
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillCommitWithoutUrlLoader().action());

  // Verify that a standard non-guest WebContents subframe does not get the
  // throttle.
  auto non_guest_wc = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), content::SiteInstance::Create(profile_.get()));
  content::RenderFrameHostTester* non_guest_rfh_tester =
      content::RenderFrameHostTester::For(non_guest_wc->GetPrimaryMainFrame());
  non_guest_rfh_tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* non_guest_child_rfh =
      non_guest_rfh_tester->AppendChild("subframe");
  content::MockNavigationHandle non_guest_handle(
      GURL("https://attacker.example"), non_guest_child_rfh);
  non_guest_handle.set_is_renderer_initiated(true);
  ASSERT_FALSE(non_guest_handle.IsInOutermostMainFrame());

  content::MockNavigationThrottleRegistry non_guest_registry(
      &non_guest_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  ContextualTasksNavigationThrottle::MaybeCreateAndAdd(non_guest_registry);
  EXPECT_TRUE(non_guest_registry.throttles().empty());
}

}  // namespace contextual_tasks

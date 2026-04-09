// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_tab_helper.h"

#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/finds/finds_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace finds {

class MockFindsServiceObserver : public FindsService::Observer {
 public:
  MockFindsServiceObserver() = default;
  MOCK_METHOD(void, OnOptInCriteriaFulfilled, (), (override));
};

class FindsTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  FindsTabHelperTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());

    // WebContentsUserData requires using CreateForWebContents.
    FindsTabHelper::CreateForWebContents(
        web_contents(), finds::FindsServiceFactory::GetForProfile(profile()),
        /*opt_guide_service=*/nullptr, template_url_service_,
        /*pref_service=*/nullptr);
    tab_helper_ = FindsTabHelper::FromWebContents(web_contents());
  }

  void TearDown() override {
    tab_helper_ = nullptr;
    template_url_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Wrapper methods to access private members of FindsTabHelper.
  void CheckSRPReturnCountAndMaybeTriggerOptIn(
      content::NavigationHandle* handle) {
    tab_helper_->CheckSRPReturnCountAndMaybeTriggerOptIn(handle);
  }

  int srp_return_count() const { return tab_helper_->srp_return_count_; }

 protected:
  raw_ptr<TemplateURLService> template_url_service_;
  raw_ptr<FindsTabHelper> tab_helper_;
};

TEST_F(FindsTabHelperTest, TestThresholdMet) {
  // Flow: 3 Forward/Back navigations to SRP.

  GURL srp_url("https://www.google.com/search?q=test");

  FindsService* service = FindsServiceFactory::GetForProfile(profile());
  MockFindsServiceObserver observer;
  service->AddObserver(&observer);

  // Expect that the service is notified when the threshold (3) is met.
  EXPECT_CALL(observer, OnOptInCriteriaFulfilled()).Times(1);

  for (int i = 0; i < 3; ++i) {
    auto handle = std::make_unique<content::MockNavigationHandle>(
        srp_url, web_contents()->GetPrimaryMainFrame());
    handle->set_page_transition(
        static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_FORWARD_BACK));
    CheckSRPReturnCountAndMaybeTriggerOptIn(handle.get());
  }

  EXPECT_EQ(srp_return_count(), 3);

  service->RemoveObserver(&observer);
}

TEST_F(FindsTabHelperTest, TestNonForwardBackNavToSRPDoesNotCount) {
  // Normal navigation (TYPED) to SRP does not count.

  GURL srp_url("https://www.google.com/search?q=test");

  auto handle = std::make_unique<content::MockNavigationHandle>(
      srp_url, web_contents()->GetPrimaryMainFrame());
  handle->set_page_transition(ui::PAGE_TRANSITION_TYPED);
  CheckSRPReturnCountAndMaybeTriggerOptIn(handle.get());

  EXPECT_EQ(srp_return_count(), 0);
}

}  // namespace finds

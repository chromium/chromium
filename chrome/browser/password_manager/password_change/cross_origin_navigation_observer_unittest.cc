// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

const char kChangePwdUrl[] = "https://www.foo.com/.well-known/change-password";

}  // namespace

class CrossOriginNavigationObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CrossOriginNavigationObserverTest() = default;
  ~CrossOriginNavigationObserverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kChangePwdUrl));
  }

  affiliations::MockAffiliationService& affiliation_service() {
    return affiliation_service_;
  }

 private:
  affiliations::MockAffiliationService affiliation_service_;
};

TEST_F(CrossOriginNavigationObserverTest, NoNavigationAfterwards) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(
      affiliation_service(),
      GetAffiliationsAndBranding(
          affiliations::FacetURI::FromCanonicalSpec("https://www.foo.com"), _))
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  task_environment()->RunUntilIdle();
}

TEST_F(CrossOriginNavigationObserverTest, NavigationInTheSameDomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  task_environment()->RunUntilIdle();

  NavigateAndCommit(GURL("https://www.foo.com/settings/"));
  NavigateAndCommit(GURL("https://www.foo.com/settings/password"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToSubdomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  task_environment()->RunUntilIdle();

  NavigateAndCommit(GURL("https://account.foo.com/settings/"));
  NavigateAndCommit(GURL("https://account.foo.com/settings/password"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToDifferentDomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  EXPECT_CALL(on_navigated_to_different_origin, Run);

  CrossOriginNavigationObserver observer(
      web_contents(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  task_environment()->RunUntilIdle();

  NavigateAndCommit(GURL("https://www.bar.com/settings/"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToAffiliatedDomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::AffiliatedFacets facets;
  facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://www.foo.com"));
  facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://www.bar.com"));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(facets, true));
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  task_environment()->RunUntilIdle();

  NavigateAndCommit(GURL("https://www.bar.com/settings/"));
}

TEST_F(CrossOriginNavigationObserverTest, DomainIsPartOfPSLExtensionList) {
  base::MockRepeatingClosure on_navigated_to_different_origin;

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{"foo.com"}));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  task_environment()->RunUntilIdle();

  NavigateAndCommit(GURL("https://www.foo.com/settings/"));
}

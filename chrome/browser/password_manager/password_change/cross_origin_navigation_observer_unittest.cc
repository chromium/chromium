// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/cross_origin_navigation_observer.h"

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
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
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>());
        barrier.Run();
      });
  EXPECT_CALL(
      affiliation_service(),
      GetAffiliationsAndBranding(
          affiliations::FacetURI::FromCanonicalSpec("https://www.foo.com"), _))
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(affiliations::AffiliatedFacets(), true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();
}

TEST_F(CrossOriginNavigationObserverTest, NavigationInTheSameDomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>());
        barrier.Run();
      });
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(affiliations::AffiliatedFacets(), true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();

  NavigateAndCommit(GURL("https://www.foo.com/settings/"));
  NavigateAndCommit(GURL("https://www.foo.com/settings/password"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToSubdomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>());
        barrier.Run();
      });
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(affiliations::AffiliatedFacets(), true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();

  NavigateAndCommit(GURL("https://account.foo.com/settings/"));
  NavigateAndCommit(GURL("https://account.foo.com/settings/password"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToDifferentDomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>());
        barrier.Run();
      });
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(affiliations::AffiliatedFacets(), true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();

  NavigateAndCommit(GURL("https://www.bar.com/settings/"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToAffiliatedDomain) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>());
        barrier.Run();
      });
  affiliations::AffiliatedFacets facets;
  facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://www.foo.com"));
  facets.emplace_back(
      affiliations::FacetURI::FromCanonicalSpec("https://www.bar.com"));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(facets, true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();

  NavigateAndCommit(GURL("https://www.bar.com/settings/"));
}

TEST_F(CrossOriginNavigationObserverTest, DomainIsPartOfPSLExtensionList) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>{"foo.com"});
        barrier.Run();
      });
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(affiliations::AffiliatedFacets(), true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run).Times(0);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();

  NavigateAndCommit(GURL("https://www.foo.com/settings/"));
}

TEST_F(CrossOriginNavigationObserverTest, NavigationToInvalidUrl) {
  base::MockRepeatingClosure on_navigated_to_different_origin;
  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce([&](auto callback) {
        std::move(callback).Run(std::vector<std::string>());
        barrier.Run();
      });
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce([&](auto uri, auto callback) {
        std::move(callback).Run(affiliations::AffiliatedFacets(), true);
        barrier.Run();
      });
  EXPECT_CALL(on_navigated_to_different_origin, Run);

  CrossOriginNavigationObserver observer(
      web_contents(), web_contents()->GetURL(), &affiliation_service(),
      on_navigated_to_different_origin.Get());
  run_loop.Run();

  NavigateAndCommit(GURL("about:blank"));
}

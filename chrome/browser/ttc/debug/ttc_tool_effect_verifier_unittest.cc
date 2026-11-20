// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_tool_effect_verifier.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {
namespace {

using Expectation = TtcToolEffectVerifier::Expectation;
using testing::HasSubstr;

constexpr base::TimeDelta kTimeout = TtcToolEffectVerifier::kTimeout;

// Arguments of an open_url call that loads `url` in the target tab.
base::DictValue OpenUrlArguments(std::string_view url, bool new_tab = false) {
  base::DictValue arguments;
  arguments.Set("url", url);
  arguments.Set("new_tab", new_tab);
  return arguments;
}

// Runs on real time: NavigationSimulator can wait in a RunLoop for other
// threads, and with mock time that idle wait fast-forwards straight to the
// verifier's timeout, which then reports instead of the navigation. Tests of
// the timeout itself use TtcToolEffectVerifierTimeoutTest and don't navigate.
class TtcToolEffectVerifierTest : public ChromeRenderViewHostTestHarness {
 protected:
  TtcToolEffectVerifierTest() = default;
  explicit TtcToolEffectVerifierTest(
      base::test::TaskEnvironment::TimeSource time_source)
      : ChromeRenderViewHostTestHarness(time_source) {}

  void TearDown() override {
    verifier_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void StartVerifier(Expectation expectation,
                     std::optional<std::string> expected_host) {
    verifier_ = std::make_unique<TtcToolEffectVerifier>(
        web_contents(), expectation, std::move(expected_host), kTimeout,
        base::DefaultTickClock::GetInstance(),
        base::BindOnce(&TtcToolEffectVerifierTest::OnResult,
                       base::Unretained(this)));
  }

  void OnResult(bool verified, std::string detail) {
    ++results_;
    verified_ = verified;
    detail_ = std::move(detail);
  }

  std::unique_ptr<TtcToolEffectVerifier> verifier_;
  int results_ = 0;
  bool verified_ = false;
  std::string detail_;
};

// For tests of the timeout, on mock time.
class TtcToolEffectVerifierTimeoutTest : public TtcToolEffectVerifierTest {
 protected:
  TtcToolEffectVerifierTimeoutTest()
      : TtcToolEffectVerifierTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(TtcToolEffectVerifierTest, ExpectationsForTools) {
  const base::DictValue no_arguments;
  EXPECT_EQ(TtcToolEffectVerifier::ExpectationForTool(
                "open_url", OpenUrlArguments("https://a.b")),
            Expectation::kNavigation);
  EXPECT_EQ(
      TtcToolEffectVerifier::ExpectationForTool("perform_search", no_arguments),
      Expectation::kNavigation);
  EXPECT_EQ(
      TtcToolEffectVerifier::ExpectationForTool("reload_page", no_arguments),
      Expectation::kNavigation);
  EXPECT_EQ(TtcToolEffectVerifier::ExpectationForTool("go_back", no_arguments),
            Expectation::kHistoryNavigation);
  EXPECT_EQ(
      TtcToolEffectVerifier::ExpectationForTool("go_forward", no_arguments),
      Expectation::kHistoryNavigation);
  EXPECT_EQ(
      TtcToolEffectVerifier::ExpectationForTool("close_tab", no_arguments),
      Expectation::kTabClosed);
  EXPECT_EQ(TtcToolEffectVerifier::ExpectationForTool("click", no_arguments),
            std::nullopt);
  // May switch to an existing tab rather than navigate.
  EXPECT_EQ(TtcToolEffectVerifier::ExpectationForTool("open_known_page",
                                                      no_arguments),
            std::nullopt);
}

// open_url in a new tab leaves the target tab alone, so there is nothing to
// check on it.
TEST_F(TtcToolEffectVerifierTest, OpenUrlInNewTabIsNotChecked) {
  EXPECT_EQ(TtcToolEffectVerifier::ExpectationForTool(
                "open_url", OpenUrlArguments("https://a.b", /*new_tab=*/true)),
            std::nullopt);
  EXPECT_EQ(
      TtcToolEffectVerifier::ExpectationForTool("open_url", base::DictValue()),
      std::nullopt);
}

TEST_F(TtcToolEffectVerifierTest, ExpectedHostComesFromOpenUrlUrl) {
  EXPECT_EQ(TtcToolEffectVerifier::ExpectedHost(
                "open_url", OpenUrlArguments("https://google.com")),
            "google.com");
  EXPECT_EQ(TtcToolEffectVerifier::ExpectedHost("open_url",
                                                OpenUrlArguments("not a url")),
            std::nullopt);
  EXPECT_EQ(TtcToolEffectVerifier::ExpectedHost(
                "perform_search", OpenUrlArguments("https://a.b")),
            std::nullopt);
}

TEST_F(TtcToolEffectVerifierTest, VerifiesNavigationToExpectedHost) {
  StartVerifier(Expectation::kNavigation, "google.com");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.google.com/search?q=x"));
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
  EXPECT_THAT(detail_, HasSubstr("visible"));
}

TEST_F(TtcToolEffectVerifierTest, SubdomainMatchesButOtherHostDoesNot) {
  StartVerifier(Expectation::kNavigation, "google.com");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://notgoogle.com/"));
  EXPECT_EQ(results_, 1);
  EXPECT_FALSE(verified_);
  EXPECT_THAT(detail_, HasSubstr("expected host google.com"));

  results_ = 0;
  StartVerifier(Expectation::kNavigation, "google.com");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://mail.google.com/"));
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
}

// A navigation that starts on the requested host counts even if it redirects
// to another one.
TEST_F(TtcToolEffectVerifierTest, RedirectFromExpectedHostIsVerified) {
  StartVerifier(Expectation::kNavigation, "gmail.com");
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gmail.com/"), web_contents());
  simulator->Start();
  simulator->Redirect(GURL("https://mail.google.com/mail/"));
  simulator->Commit();
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
  EXPECT_THAT(detail_, HasSubstr("https://mail.google.com/mail/"));
}

TEST_F(TtcToolEffectVerifierTest, WwwPrefixIsIgnored) {
  EXPECT_EQ(TtcToolEffectVerifier::ExpectedHost(
                "open_url", OpenUrlArguments("https://www.google.com/")),
            "www.google.com");
  StartVerifier(Expectation::kNavigation, "www.google.com");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://google.com/"));
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
}

TEST_F(TtcToolEffectVerifierTest, IgnoresNavigationAlreadyInFlight) {
  auto in_flight = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  in_flight->Start();
  StartVerifier(Expectation::kNavigation, "example.org");
  in_flight->Commit();
  EXPECT_EQ(results_, 0);

  // The tool's own navigation is judged.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.org/"));
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
}

TEST_F(TtcToolEffectVerifierTest, SameDocumentOnlyCountsForHistory) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/"));

  // A fragment change isn't the page load an open_url tool expects.
  StartVerifier(Expectation::kNavigation, std::nullopt);
  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/#a"), main_rfh())
      ->CommitSameDocument();
  EXPECT_EQ(results_, 0);

  // But going back in a single-page app is a same-document navigation.
  StartVerifier(Expectation::kHistoryNavigation, std::nullopt);
  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/#b"), main_rfh())
      ->CommitSameDocument();
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
}

TEST_F(TtcToolEffectVerifierTest, NavigationInHiddenTabFails) {
  web_contents()->WasHidden();
  StartVerifier(Expectation::kNavigation, std::nullopt);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/"));
  EXPECT_EQ(results_, 1);
  EXPECT_FALSE(verified_);
  EXPECT_THAT(detail_, HasSubstr("isn't visible"));
}

TEST_F(TtcToolEffectVerifierTest, ErrorPageFails) {
  StartVerifier(Expectation::kNavigation, std::nullopt);
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://unresolvable.test/"), web_contents());
  simulator->Fail(net::ERR_NAME_NOT_RESOLVED);
  simulator->CommitErrorPage();
  EXPECT_EQ(results_, 1);
  EXPECT_FALSE(verified_);
  EXPECT_THAT(detail_, HasSubstr("error page"));
}

// An aborted navigation commits nothing, so the verifier keeps waiting.
TEST_F(TtcToolEffectVerifierTest, AbortedNavigationKeepsWaiting) {
  StartVerifier(Expectation::kNavigation, std::nullopt);
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  simulator->Fail(net::ERR_ABORTED);
  EXPECT_EQ(results_, 0);
}

TEST_F(TtcToolEffectVerifierTimeoutTest, NoNavigationTimesOut) {
  StartVerifier(Expectation::kNavigation, std::nullopt);
  task_environment()->FastForwardBy(kTimeout - base::Milliseconds(1));
  EXPECT_EQ(results_, 0);
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(results_, 1);
  EXPECT_FALSE(verified_);
  EXPECT_THAT(detail_, HasSubstr("no navigation committed"));
}

TEST_F(TtcToolEffectVerifierTest, TabClosedIsVerified) {
  StartVerifier(Expectation::kTabClosed, std::nullopt);
  DeleteContents();
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
}

TEST_F(TtcToolEffectVerifierTest,
       WebContentsDestroyedWhileExpectingNavigation) {
  StartVerifier(Expectation::kNavigation, std::nullopt);
  DeleteContents();
  EXPECT_EQ(results_, 1);
  EXPECT_FALSE(verified_);
  EXPECT_THAT(detail_, HasSubstr("closed before the navigation committed"));
}

TEST_F(TtcToolEffectVerifierTimeoutTest, TabStillOpenTimesOut) {
  StartVerifier(Expectation::kTabClosed, std::nullopt);
  task_environment()->FastForwardBy(kTimeout);
  EXPECT_EQ(results_, 1);
  EXPECT_FALSE(verified_);
  EXPECT_THAT(detail_, HasSubstr("still open"));
}

TEST_F(TtcToolEffectVerifierTest, ReportsOnlyOnce) {
  StartVerifier(Expectation::kNavigation, std::nullopt);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/"));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.org/"));
  EXPECT_EQ(results_, 1);
}

// Once the effect is reported, the timeout doesn't report again.
TEST_F(TtcToolEffectVerifierTimeoutTest, NoTimeoutAfterTheEffect) {
  StartVerifier(Expectation::kTabClosed, std::nullopt);
  DeleteContents();
  ASSERT_EQ(results_, 1);
  task_environment()->FastForwardBy(kTimeout);
  EXPECT_EQ(results_, 1);
  EXPECT_TRUE(verified_);
}

}  // namespace
}  // namespace ttc

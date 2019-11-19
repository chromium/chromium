// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Not;
using ::testing::Optional;

const char kSimpleArticlePath[] = "/dom_distiller/simple_article.html";
const char kSimpleArticleIFramePath[] =
    "/dom_distiller/simple_article_iframe.html";
const char kArticlePath[] = "/dom_distiller/og_article.html";
const char kNonArticlePath[] = "/dom_distiller/non_og_article.html";

const char* kAllPaths[] = {kSimpleArticlePath, kSimpleArticleIFramePath,
                           kArticlePath, kNonArticlePath};

class MockObserver : public DistillabilityObserver {
 public:
  MOCK_METHOD1(OnResult, void(const DistillabilityResult& result));
};

// Wait a bit to make sure there are no extra calls after the last expected
// call. All the expected calls happen within ~1ms on linux release build,
// so 100ms should be pretty safe to catch extra calls.
//
// If there are no extra calls, changing this doesn't change the test result.
const auto kWaitAfterLastCall = base::TimeDelta::FromMilliseconds(100);

// Wait a bit if no calls are expected to make sure any unexpected calls are
// caught. Expected calls happen within 100ms after content::WaitForLoadStop()
// on linux release build, so 1s provides a safe margin.
//
// If there are no extra calls, changing this doesn't change the test result.
const auto kWaitNoExpectedCall = base::TimeDelta::FromSeconds(1);

}  // namespace

template <const char Option[]>
class TestOption : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
    command_line->AppendSwitchASCII(switches::kReaderModeHeuristics, Option);
    command_line->AppendSwitch(switches::kEnableDistillabilityService);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    AddObserver(web_contents_, &holder_);
  }

  void NavigateAndWait(const char* url, base::TimeDelta test_timeout) {
    run_loop_ = std::make_unique<base::RunLoop>();

    GURL article_url(url);
    if (base::StartsWith(url, "/", base::CompareCase::SENSITIVE)) {
      article_url = embedded_test_server()->GetURL(url);
    }

    // This blocks until the navigation has completely finished.
    ui_test_utils::NavigateToURL(browser(), article_url);
    content::WaitForLoadStop(web_contents_);

    if (!test_timeout.is_zero())
      QuitAfter(test_timeout);

    run_loop_->Run();
    run_loop_.reset();
  }

  void QuitSoon() { QuitAfter(kWaitAfterLastCall); }

  void QuitAfter(base::TimeDelta delta) {
    DCHECK(delta > base::TimeDelta());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), delta);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  MockObserver holder_;
  content::WebContents* web_contents_ = nullptr;
};

MATCHER(IsDistillable,
        "Result " + std::string(negation ? "isn't" : "is") + " distillable") {
  return Matches(Field(&DistillabilityResult::is_distillable, true))(arg);
}

MATCHER(IsLast, "Result " + std::string(negation ? "isn't" : "is") + " last") {
  return Matches(Field(&DistillabilityResult::is_last, true))(arg);
}

MATCHER(IsMobileFriendly,
        "Result " + std::string(negation ? "isn't" : "is") +
            " mobile friendly") {
  return Matches(Field(&DistillabilityResult::is_mobile_friendly, true))(arg);
}

using DistillablePageUtilsBrowserTestAlways =
    TestOption<switches::reader_mode_heuristics::kAlwaysTrue>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAlways,
                       AllRealPathsCallObserverOnceWithIsDistillable) {
  for (unsigned i = 0; i < sizeof(kAllPaths) / sizeof(kAllPaths[0]); ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(AllOf(IsDistillable(), IsLast())))
        .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
    NavigateAndWait(kAllPaths[i], base::TimeDelta());
    EXPECT_THAT(GetLatestResult(web_contents_),
                Optional(AllOf(IsDistillable(), IsLast())));
  }
}

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAlways,
                       LocalUrlsDoNotCallObserver) {
  EXPECT_CALL(holder_, OnResult(_)).Times(0);
  NavigateAndWait("about:blank", kWaitNoExpectedCall);
  EXPECT_EQ(GetLatestResult(web_contents_), base::nullopt);
}

using DistillablePageUtilsBrowserTestNone =
    TestOption<switches::reader_mode_heuristics::kNone>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestNone, NeverCallObserver) {
  EXPECT_CALL(holder_, OnResult(_)).Times(0);
  NavigateAndWait(kSimpleArticlePath, kWaitNoExpectedCall);
  EXPECT_EQ(GetLatestResult(web_contents_), base::nullopt);
}

using DistillablePageUtilsBrowserTestOGArticle =
    TestOption<switches::reader_mode_heuristics::kOGArticle>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestOGArticle,
                       ArticlesCallObserverOnceWithIsDistillable) {
  EXPECT_CALL(holder_, OnResult(AllOf(IsDistillable(), IsLast())))
      .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
  NavigateAndWait(kArticlePath, base::TimeDelta());
  EXPECT_THAT(GetLatestResult(web_contents_),
              Optional(AllOf(IsDistillable(), IsLast())));
}

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestOGArticle,
                       NonArticleCallsObserverOnceWithIsNotDistillable) {
  EXPECT_CALL(holder_, OnResult(AllOf(Not(IsDistillable()), IsLast())))
      .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
  NavigateAndWait(kNonArticlePath, base::TimeDelta());
  EXPECT_THAT(GetLatestResult(web_contents_),
              Optional(AllOf(Not(IsDistillable()), IsLast())));
}

using DistillablePageUtilsBrowserTestAdaboost =
    TestOption<switches::reader_mode_heuristics::kAdaBoost>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAdaboost,
                       SimpleArticlesCallObserverTwiceWithIsDistillable) {
  const char* paths[] = {kSimpleArticlePath, kSimpleArticleIFramePath};
  for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(AllOf(IsDistillable(), Not(IsLast()),
                                        Not(IsMobileFriendly()))))
        .Times(1);
    EXPECT_CALL(holder_, OnResult(AllOf(IsDistillable(), IsLast(),
                                        Not(IsMobileFriendly()))))
        .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
    NavigateAndWait(paths[i], base::TimeDelta());

    EXPECT_THAT(
        GetLatestResult(web_contents_),
        Optional(AllOf(IsDistillable(), IsLast(), Not(IsMobileFriendly()))));
  }
}

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAdaboost,
                       NonArticleCallsObserverTwiceWithIsNotDistillable) {
  testing::InSequence dummy;
  EXPECT_CALL(holder_, OnResult(AllOf(Not(IsDistillable()), Not(IsLast()),
                                      Not(IsMobileFriendly()))))
      .Times(1);
  EXPECT_CALL(holder_, OnResult(AllOf(Not(IsDistillable()), IsLast(),
                                      Not(IsMobileFriendly()))))
      .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
  NavigateAndWait(kNonArticlePath, base::TimeDelta());
  EXPECT_THAT(
      GetLatestResult(web_contents_),
      Optional(AllOf(Not(IsDistillable()), IsLast(), Not(IsMobileFriendly()))));
}

using DistillablePageUtilsBrowserTestAllArticles =
    TestOption<switches::reader_mode_heuristics::kAllArticles>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAllArticles,
                       SimpleArticlesCallObserverTwiceWithIsDistillable) {
  const char* paths[] = {kSimpleArticlePath, kSimpleArticleIFramePath};
  for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(AllOf(IsDistillable(), Not(IsLast()),
                                        Not(IsMobileFriendly()))))
        .Times(1);
    EXPECT_CALL(holder_, OnResult(AllOf(IsDistillable(), IsLast(),
                                        Not(IsMobileFriendly()))))
        .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
    NavigateAndWait(paths[i], base::TimeDelta());
    EXPECT_THAT(
        GetLatestResult(web_contents_),
        Optional(AllOf(IsDistillable(), IsLast(), Not(IsMobileFriendly()))));
  }
}

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAllArticles,
                       NonArticlesCallObserverTwiceWithIsNotDistillable) {
  testing::InSequence dummy;
  EXPECT_CALL(holder_, OnResult(AllOf(Not(IsDistillable()), Not(IsLast()),
                                      Not(IsMobileFriendly()))))
      .Times(1);
  EXPECT_CALL(holder_, OnResult(AllOf(Not(IsDistillable()), IsLast(),
                                      Not(IsMobileFriendly()))))
      .WillOnce(testing::InvokeWithoutArgs(this, &TestOption::QuitSoon));
  NavigateAndWait(kNonArticlePath, base::TimeDelta());
  EXPECT_THAT(
      GetLatestResult(web_contents_),
      Optional(AllOf(Not(IsDistillable()), IsLast(), Not(IsMobileFriendly()))));
}

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAllArticles,
                       ObserverNotCalledAfterRemoval) {
  RemoveObserver(web_contents_, &holder_);
  EXPECT_CALL(holder_, OnResult(_)).Times(0);
  NavigateAndWait(kSimpleArticlePath, kWaitNoExpectedCall);
  EXPECT_THAT(
      GetLatestResult(web_contents_),
      Optional(AllOf(IsDistillable(), IsLast(), Not(IsMobileFriendly()))));
}

}  // namespace dom_distiller

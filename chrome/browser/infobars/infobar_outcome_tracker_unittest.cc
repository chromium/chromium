// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_outcome_tracker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace infobars {
namespace {

TEST(InfoBarOutcomeTrackerTest, UnarmedTrackerReportsNothing) {
  base::HistogramTester histograms;
  bool called = false;

  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindLambdaForTesting(
            [&](content::WebContents*, InfoBarResult) { called = true; }));
    tracker->MarkLinkClicked();
    EXPECT_FALSE(tracker->Claim(InfoBarResult::kAccepted));
    tracker->ReportPending(/*web_contents=*/nullptr);
  }

  EXPECT_FALSE(called);
  histograms.ExpectTotalCount("InfoBar.Centralized.Ignored", 0);
}

TEST(InfoBarOutcomeTrackerTest, ArmedTrackerReportsIgnoredWhenLastRefDrops) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;

  {
    auto ref1 = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindLambdaForTesting(
            [&](content::WebContents*, InfoBarResult result) {
              results.push_back(result);
            }));
    ref1->Arm();
    {
      scoped_refptr<InfoBarOutcomeTracker> ref2 = ref1;
    }
    EXPECT_TRUE(results.empty());
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kIgnored, results[0]);
  histograms.ExpectUniqueSample("InfoBar.Centralized.Ignored",
                                InfoBarDelegate::TEST_INFOBAR, 1);
}

TEST(InfoBarOutcomeTrackerTest, LinkClickUpgradesFallbackWithoutIgnoredMetric) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;

  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindLambdaForTesting(
            [&](content::WebContents*, InfoBarResult result) {
              results.push_back(result);
            }));
    tracker->Arm();
    tracker->MarkLinkClicked();
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kLinkClicked, results[0]);
  histograms.ExpectTotalCount("InfoBar.Centralized.Ignored", 0);
}

TEST(InfoBarOutcomeTrackerTest, ClaimOverridesPendingLinkClick) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;

  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindLambdaForTesting(
            [&](content::WebContents*, InfoBarResult result) {
              results.push_back(result);
            }));
    tracker->Arm();
    tracker->MarkLinkClicked();
    auto callback = tracker->Claim(InfoBarResult::kAccepted);
    ASSERT_TRUE(callback);
    tracker->ReportPending(/*web_contents=*/nullptr);
    std::move(callback).Run(/*web_contents=*/nullptr);
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kAccepted, results[0]);
  histograms.ExpectTotalCount("InfoBar.Centralized.Ignored", 0);
}

TEST(InfoBarOutcomeTrackerTest, ClaimAndSuppressPreventDuplicateReporting) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;
  auto record = base::BindLambdaForTesting(
      [&](content::WebContents*, InfoBarResult result) {
        results.push_back(result);
      });

  {
    auto claimed = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    claimed->Arm();
    auto callback = claimed->Claim(InfoBarResult::kAccepted);
    ASSERT_TRUE(callback);
    EXPECT_FALSE(claimed->Claim(InfoBarResult::kDismissed));
    claimed->MarkLinkClicked();
    claimed->ReportPending(/*web_contents=*/nullptr);
    std::move(callback).Run(/*web_contents=*/nullptr);
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kAccepted, results[0]);

  {
    bool bound_state_alive = true;
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindRepeating(
            [](base::ScopedClosureRunner*, content::WebContents*,
               InfoBarResult) { ADD_FAILURE(); },
            base::Owned(std::make_unique<base::ScopedClosureRunner>(
                base::BindLambdaForTesting(
                    [&] { bound_state_alive = false; })))));
    tracker->Arm();
    tracker->Suppress();
    EXPECT_FALSE(bound_state_alive);
    EXPECT_FALSE(tracker->Claim(InfoBarResult::kAccepted));
    tracker->ReportPending(/*web_contents=*/nullptr);
  }

  EXPECT_EQ(1u, results.size());
  histograms.ExpectTotalCount("InfoBar.Centralized.Ignored", 0);
}

TEST(InfoBarOutcomeTrackerTest, ReportPendingReportsAtMostOnce) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;

  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindLambdaForTesting(
            [&](content::WebContents*, InfoBarResult result) {
              results.push_back(result);
            }));
    tracker->Arm();
    tracker->ReportPending(/*web_contents=*/nullptr);
    tracker->ReportPending(/*web_contents=*/nullptr);
    EXPECT_FALSE(tracker->Claim(InfoBarResult::kAccepted));
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kIgnored, results[0]);
  histograms.ExpectUniqueSample("InfoBar.Centralized.Ignored",
                                InfoBarDelegate::TEST_INFOBAR, 1);
}

TEST(InfoBarOutcomeTrackerTest, ReportPendingAfterLinkClickAndSuppress) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;
  auto record = base::BindLambdaForTesting(
      [&](content::WebContents*, InfoBarResult result) {
        results.push_back(result);
      });

  {
    auto link_clicked = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    link_clicked->Arm();
    link_clicked->MarkLinkClicked();
    link_clicked->ReportPending(/*web_contents=*/nullptr);
    link_clicked->ReportPending(/*web_contents=*/nullptr);
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kLinkClicked, results[0]);

  {
    auto suppressed = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    suppressed->Arm();
    suppressed->Suppress();
    suppressed->ReportPending(/*web_contents=*/nullptr);
  }

  {
    auto suppressed_after_link = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    suppressed_after_link->Arm();
    suppressed_after_link->MarkLinkClicked();
    suppressed_after_link->Suppress();
    suppressed_after_link->ReportPending(/*web_contents=*/nullptr);
  }

  EXPECT_EQ(1u, results.size());
  histograms.ExpectTotalCount("InfoBar.Centralized.Ignored", 0);
}

TEST(InfoBarOutcomeTrackerTest, RepeatedArmDoesNotDowngradeOrRearm) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;
  auto record = base::BindLambdaForTesting(
      [&](content::WebContents*, InfoBarResult result) {
        results.push_back(result);
      });

  // Calling Arm() after MarkLinkClicked() preserves kLinkClicked.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
    tracker->MarkLinkClicked();
    tracker->Arm();
  }

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kLinkClicked, results[0]);

  // Calling Arm() after Claim() does not re-arm.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
    auto callback = tracker->Claim(InfoBarResult::kAccepted);
    ASSERT_TRUE(callback);
    tracker->Arm();
    std::move(callback).Run(/*web_contents=*/nullptr);
  }

  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(InfoBarResult::kAccepted, results[1]);

  // Calling Arm() after Suppress() does not re-arm.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
    tracker->Suppress();
    tracker->Arm();
  }

  // Calling Suppress() while unarmed prevents a subsequent Arm().
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Suppress();
    tracker->Arm();
  }

  // Calling Arm() after ReportPending() does not re-arm.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
    tracker->ReportPending(/*web_contents=*/nullptr);
    tracker->Arm();
  }

  ASSERT_EQ(3u, results.size());
  EXPECT_EQ(InfoBarResult::kIgnored, results[2]);
  histograms.ExpectUniqueSample("InfoBar.Centralized.Ignored",
                                InfoBarDelegate::TEST_INFOBAR, 1);
}

TEST(InfoBarOutcomeTrackerTest, SelfDestructionDuringCallbackIsSafe) {
  base::HistogramTester histograms;

  // ReportPending() dropping the last reference mid-callback keeps the
  // callback's bound state alive until Run() finishes.
  {
    int observed_value = 0;
    scoped_refptr<InfoBarOutcomeTracker> tracker;
    tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindRepeating(
            [](scoped_refptr<InfoBarOutcomeTracker>* owner, int* state,
               int* out, content::WebContents*, InfoBarResult) {
              owner->reset();
              *out = *state;
            },
            &tracker, base::Owned(std::make_unique<int>(42)), &observed_value));
    tracker->Arm();
    tracker->ReportPending(/*web_contents=*/nullptr);
    EXPECT_EQ(nullptr, tracker);
    EXPECT_EQ(42, observed_value);
  }

  // Claim()ed callback remains safe to run even after the tracker is destroyed.
  {
    int observed_value = 0;
    InfoBarOutcomeTracker::ClaimedCallback claimed_callback;
    {
      auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
          InfoBarDelegate::TEST_INFOBAR,
          base::BindRepeating([](int* state, int* out, content::WebContents*,
                                 InfoBarResult) { *out = *state; },
                              base::Owned(std::make_unique<int>(99)),
                              &observed_value));
      tracker->Arm();
      claimed_callback = tracker->Claim(InfoBarResult::kAccepted);
    }
    ASSERT_TRUE(claimed_callback);
    std::move(claimed_callback).Run(/*web_contents=*/nullptr);
    EXPECT_EQ(99, observed_value);
  }

  // Suppress() dropping the last reference via bound callback state is safe.
  {
    bool callback_destroyed = false;
    scoped_refptr<InfoBarOutcomeTracker> tracker;
    auto on_destroy = std::make_unique<base::ScopedClosureRunner>(
        base::BindLambdaForTesting([&] {
          tracker.reset();
          callback_destroyed = true;
        }));
    tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR,
        base::BindRepeating(
            [](base::ScopedClosureRunner*, content::WebContents*,
               InfoBarResult) { ADD_FAILURE(); },
            base::Owned(std::move(on_destroy))));
    tracker->Arm();
    tracker->Suppress();
    EXPECT_EQ(nullptr, tracker);
    EXPECT_TRUE(callback_destroyed);
  }

  histograms.ExpectUniqueSample("InfoBar.Centralized.Ignored",
                                InfoBarDelegate::TEST_INFOBAR, 1);
}

TEST(InfoBarOutcomeTrackerTest, ReentrancyInsideReportPendingIsSafe) {
  base::HistogramTester histograms;
  std::vector<InfoBarResult> results;
  scoped_refptr<InfoBarOutcomeTracker> tracker;
  tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
      InfoBarDelegate::TEST_INFOBAR,
      base::BindLambdaForTesting(
          [&](content::WebContents*, InfoBarResult result) {
            results.push_back(result);
            EXPECT_FALSE(tracker->Claim(InfoBarResult::kAccepted));
            tracker->MarkLinkClicked();
            tracker->ReportPending(/*web_contents=*/nullptr);
            tracker->Suppress();
          }));
  tracker->Arm();
  tracker->ReportPending(/*web_contents=*/nullptr);

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kIgnored, results[0]);
  histograms.ExpectUniqueSample("InfoBar.Centralized.Ignored",
                                InfoBarDelegate::TEST_INFOBAR, 1);
}

TEST(InfoBarOutcomeTrackerTest, NullCallbackDoesNotCrashAndRecordsIgnored) {
  base::HistogramTester histograms;

  // ReportPending() records Ignored even with a null callback.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, base::NullCallback());
    tracker->Arm();
    tracker->ReportPending(/*web_contents=*/nullptr);
  }

  // Destructor fallback records Ignored even with a null callback.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, base::NullCallback());
    tracker->Arm();
  }

  // Claim() resolves the tracker without recording Ignored even with a null
  // callback.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, base::NullCallback());
    tracker->Arm();
    EXPECT_FALSE(tracker->Claim(InfoBarResult::kAccepted));
  }

  // ReportPending() after MarkLinkClicked() with a null callback does not crash
  // or record Ignored.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, base::NullCallback());
    tracker->Arm();
    tracker->MarkLinkClicked();
    tracker->ReportPending(/*web_contents=*/nullptr);
  }

  histograms.ExpectUniqueSample("InfoBar.Centralized.Ignored",
                                InfoBarDelegate::TEST_INFOBAR, 2);
}

TEST(InfoBarOutcomeTrackerTest, ForwardsWebContentsToCallback) {
  content::BrowserTaskEnvironment task_environment;
  content::RenderViewHostTestEnabler rvh_enabler;
  TestingProfile profile;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile, nullptr);

  std::vector<std::pair<content::WebContents*, InfoBarResult>> calls;
  auto record = base::BindLambdaForTesting(
      [&](content::WebContents* contents, InfoBarResult result) {
        calls.emplace_back(contents, result);
      });

  // Claim() forwards the caller-supplied WebContents*.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
    auto callback = tracker->Claim(InfoBarResult::kAccepted);
    ASSERT_TRUE(callback);
    std::move(callback).Run(web_contents.get());
  }

  // ReportPending() forwards the caller-supplied WebContents*.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
    tracker->ReportPending(web_contents.get());
  }

  // Destructor fallback passes nullptr.
  {
    auto tracker = base::MakeRefCounted<InfoBarOutcomeTracker>(
        InfoBarDelegate::TEST_INFOBAR, record);
    tracker->Arm();
  }

  ASSERT_EQ(3u, calls.size());
  EXPECT_EQ(web_contents.get(), calls[0].first);
  EXPECT_EQ(InfoBarResult::kAccepted, calls[0].second);
  EXPECT_EQ(web_contents.get(), calls[1].first);
  EXPECT_EQ(InfoBarResult::kIgnored, calls[1].second);
  EXPECT_EQ(nullptr, calls[2].first);
  EXPECT_EQ(InfoBarResult::kIgnored, calls[2].second);
}

}  // namespace
}  // namespace infobars

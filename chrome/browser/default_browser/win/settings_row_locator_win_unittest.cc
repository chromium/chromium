// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/win/settings_row_locator_win.h"

#include <objbase.h>

#include <windows.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/win/uia_element_locator_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

#include <uiautomation.h>

namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using FailureReason = SettingsRowLocatorWin::FailureReason;
using Measurement = UiaElementLocatorWin::Measurement;
using Outcome = SettingsRowLocatorWin::Outcome;
using MeasureFuture = base::test::TestFuture<SettingsRowLocatorWin::Result>;
using ButtonFuture =
    base::test::TestFuture<SettingsRowLocatorWin::ButtonResult>;

constexpr char kOutcomeHistogram[] =
    "DefaultBrowser.VisualGuide.RowMeasurementOutcome";
constexpr char kDurationHistogram[] =
    "DefaultBrowser.VisualGuide.RowMeasurementTime";

const HWND kHwnd = reinterpret_cast<HWND>(0x1234);
constexpr gfx::Rect kWindowBounds(100, 200, 300, 400);
// Center 120px below the window's top edge.
constexpr gfx::Rect kRowBounds(120, 300, 200, 40);

base::Process OpenCurrentProcess() {
  return base::Process::OpenWithAccess(::GetCurrentProcessId(),
                                       PROCESS_QUERY_LIMITED_INFORMATION);
}

// Captures the request and answers on demand. Like the real locator, answers
// a request still in flight with kSuperseded, from a task, when a newer one
// arrives.
class FakeLocator : public UiaElementLocatorWin {
 public:
  void Locate(Target target,
              const Query& query,
              LocateCallback callback) override {
    ++call_count_;
    target_ = std::move(target);
    query_ = query;
    LocateCallback superseded = std::exchange(callback_, std::move(callback));
    if (superseded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(superseded),
                         base::unexpected(FailureReason::kSuperseded)));
    }
  }

  void Answer(UiaElementLocatorWin::Result result) {
    std::move(callback_).Run(std::move(result));
  }

  int call_count() const { return call_count_; }
  const Target& target() const { return target_; }
  const Query& query() const { return query_; }

 private:
  int call_count_ = 0;
  Target target_;
  Query query_;
  LocateCallback callback_;
};

class SettingsRowLocatorWinTest : public testing::Test {
 protected:
  SettingsRowLocatorWinTest() {
    auto fake = std::make_unique<FakeLocator>();
    fake_ = fake.get();
    locator_ = std::make_unique<SettingsRowLocatorWin>(std::move(fake));
  }

  void Measure(MeasureFuture& future) {
    locator_->Measure(kHwnd, OpenCurrentProcess(), future.GetCallback());
  }

  void MeasureButton(ButtonFuture& future) {
    locator_->MeasureButton(kHwnd, OpenCurrentProcess(), future.GetCallback());
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histograms_;
  std::unique_ptr<SettingsRowLocatorWin> locator_;
  // Owned by `locator_`, so declared after it.
  raw_ptr<FakeLocator> fake_ = nullptr;
};

TEST_F(SettingsRowLocatorWinTest, AsksForTheSettingsRow) {
  base::Process process = OpenCurrentProcess();
  const HANDLE handle = process.Handle();
  MeasureFuture future;
  locator_->Measure(kHwnd, std::move(process), future.GetCallback());

  EXPECT_EQ(fake_->call_count(), 1);
  EXPECT_EQ(fake_->target().hwnd, kHwnd);
  // The handle is handed on as is.
  EXPECT_EQ(fake_->target().process.Handle(), handle);
  EXPECT_EQ(fake_->target().window_class, L"ApplicationFrameWindow");
  EXPECT_EQ(fake_->query().automation_id,
            L"SystemSettings_DefaultApps_"
            L"DefaultBrowserWithPinToStartAndTaskbarAction_Button");
  EXPECT_EQ(fake_->query().fallback_automation_id_fragment, L"defaultbrowser");
  EXPECT_EQ(fake_->query().fallback_control_type, UIA_ButtonControlTypeId);
  // Settings can land on another page first and navigate to the browser's.
  EXPECT_EQ(fake_->query().wait_for, base::Seconds(10));
}

TEST_F(SettingsRowLocatorWinTest, OffsetIsFromWindowTopToRowCenter) {
  MeasureFuture future;
  Measure(future);
  fake_->Answer(Measurement{kRowBounds, kWindowBounds});

  EXPECT_THAT(future.Get(), ValueIs(120));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kLocated, 1);
  histograms_.ExpectTotalCount(kDurationHistogram, 1);
}

TEST_F(SettingsRowLocatorWinTest, RowOutsideTheRangeIsUnusable) {
  MeasureFuture future;
  Measure(future);
  fake_->Answer(Measurement{gfx::Rect(120, 200, 200, 0), kWindowBounds});

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kUnusable));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kUnusable, 1);
}

// The window spans y 200-600, so a row centered at 620 is below it.
TEST_F(SettingsRowLocatorWinTest, RowBelowTheWindowIsUnusable) {
  MeasureFuture future;
  Measure(future);
  fake_->Answer(Measurement{gfx::Rect(120, 600, 200, 40), kWindowBounds});

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kUnusable));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kUnusable, 1);
}

TEST_F(SettingsRowLocatorWinTest, PassesFailuresThrough) {
  MeasureFuture future;
  Measure(future);
  fake_->Answer(base::unexpected(FailureReason::kTimedOut));

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kTimedOut));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kTimedOut, 1);
  histograms_.ExpectTotalCount(kDurationHistogram, 1);
}

TEST_F(SettingsRowLocatorWinTest, WindowGoneIsRecordedButNotTimed) {
  MeasureFuture future;
  Measure(future);
  fake_->Answer(base::unexpected(FailureReason::kWindowGone));

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kWindowGone));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kWindowGone, 1);
  histograms_.ExpectTotalCount(kDurationHistogram, 0);
}

TEST_F(SettingsRowLocatorWinTest, SupersededIsRecordedButNotTimed) {
  MeasureFuture first;
  Measure(first);
  MeasureFuture second;
  Measure(second);

  EXPECT_THAT(first.Get(), ErrorIs(FailureReason::kSuperseded));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kSuperseded, 1);
  histograms_.ExpectTotalCount(kDurationHistogram, 0);

  fake_->Answer(Measurement{kRowBounds, kWindowBounds});
  EXPECT_THAT(second.Get(), ValueIs(120));
  histograms_.ExpectBucketCount(kOutcomeHistogram, Outcome::kLocated, 1);
}

TEST_F(SettingsRowLocatorWinTest, DestructionAbandonsTheMeasurementInFlight) {
  MeasureFuture first;
  Measure(first);
  // Superseded, so its answer is already on its way when `locator_` goes.
  MeasureFuture second;
  Measure(second);
  ASSERT_FALSE(first.IsReady());

  fake_ = nullptr;
  locator_.reset();
  base::test::TestFuture<void> flushed;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flushed.GetCallback());
  ASSERT_TRUE(flushed.Wait());

  EXPECT_FALSE(first.IsReady());
  EXPECT_FALSE(second.IsReady());
  histograms_.ExpectTotalCount(kOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kDurationHistogram, 0);
}

TEST_F(SettingsRowLocatorWinTest, MeasureButtonAsksForTheSameButton) {
  MeasureFuture row_future;
  Measure(row_future);
  const UiaElementLocatorWin::Query row_query = fake_->query();
  fake_->Answer(base::unexpected(FailureReason::kTimedOut));

  ButtonFuture future;
  MeasureButton(future);

  EXPECT_EQ(fake_->call_count(), 2);
  EXPECT_EQ(fake_->target().hwnd, kHwnd);
  ASSERT_TRUE(fake_->target().process.IsValid());
  EXPECT_EQ(fake_->target().window_class, L"ApplicationFrameWindow");
  EXPECT_EQ(fake_->query().automation_id, row_query.automation_id);
  EXPECT_EQ(fake_->query().fallback_automation_id_fragment,
            row_query.fallback_automation_id_fragment);
  EXPECT_EQ(fake_->query().fallback_control_type,
            row_query.fallback_control_type);
  EXPECT_EQ(fake_->query().wait_for, row_query.wait_for);
}

TEST_F(SettingsRowLocatorWinTest, ButtonBoundsAreRelativeToTheWindow) {
  ButtonFuture future;
  MeasureButton(future);
  fake_->Answer(Measurement{kRowBounds, kWindowBounds});

  EXPECT_THAT(future.Get(), ValueIs(gfx::Rect(20, 100, 200, 40)));
  histograms_.ExpectTotalCount(kOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kDurationHistogram, 0);
}

// Centered at x 450, outside the window's x 100-400.
TEST_F(SettingsRowLocatorWinTest, ButtonOutsideTheWindowIsUnusable) {
  ButtonFuture future;
  MeasureButton(future);
  fake_->Answer(Measurement{gfx::Rect(400, 300, 100, 40), kWindowBounds});

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kUnusable));
}

// Inside the window, but nothing to point at.
TEST_F(SettingsRowLocatorWinTest, EmptyButtonBoundsAreUnusable) {
  ButtonFuture future;
  MeasureButton(future);
  fake_->Answer(Measurement{gfx::Rect(120, 300, 0, 0), kWindowBounds});

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kUnusable));
}

TEST_F(SettingsRowLocatorWinTest, MeasureButtonPassesFailuresThrough) {
  ButtonFuture future;
  MeasureButton(future);
  fake_->Answer(base::unexpected(FailureReason::kWindowGone));

  EXPECT_THAT(future.Get(), ErrorIs(FailureReason::kWindowGone));
  histograms_.ExpectTotalCount(kOutcomeHistogram, 0);
}

// Both entry points go through the one locator, which keeps one request.
TEST_F(SettingsRowLocatorWinTest, MeasureButtonSupersedesARowMeasurement) {
  MeasureFuture row_future;
  Measure(row_future);
  ButtonFuture future;
  MeasureButton(future);

  EXPECT_THAT(row_future.Get(), ErrorIs(FailureReason::kSuperseded));
  histograms_.ExpectUniqueSample(kOutcomeHistogram, Outcome::kSuperseded, 1);

  fake_->Answer(Measurement{kRowBounds, kWindowBounds});
  EXPECT_THAT(future.Get(), ValueIs(gfx::Rect(20, 100, 200, 40)));
  // The button measurement adds nothing to the arrow flow's histograms.
  histograms_.ExpectTotalCount(kOutcomeHistogram, 1);
}

}  // namespace

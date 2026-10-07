// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/window_presentation_metrics_recorder.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ui/base/app_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr char kHistogramPrefix[] = "Ash.Window.InitToFirstPresentation.";
constexpr char kBrowserHistogram[] =
    "Ash.Window.InitToFirstPresentation.Browser";
constexpr char kArcAppHistogram[] = "Ash.Window.InitToFirstPresentation.ArcApp";

// Shell owns the recorder, so these tests exercise the production instance.
class WindowPresentationMetricsRecorderTest : public AshTestBase {
 protected:
  void WaitForPresentation() {
    ASSERT_TRUE(ui::WaitForNextFrameToBePresented(
        Shell::GetPrimaryRootWindow()->GetHost()->compositor()));
  }

  base::HistogramTester histograms_;
};

}  // namespace

TEST_F(WindowPresentationMetricsRecorderTest, RecordsBrowserWindow) {
  std::unique_ptr<aura::Window> window =
      CreateWindowWithAppType(chromeos::AppType::BROWSER);
  WaitForPresentation();
  histograms_.ExpectTotalCount(kBrowserHistogram, 1);
}

TEST_F(WindowPresentationMetricsRecorderTest, RecordsPerAppType) {
  std::unique_ptr<aura::Window> window =
      CreateWindowWithAppType(chromeos::AppType::ARC_APP);
  WaitForPresentation();
  histograms_.ExpectTotalCount(kArcAppHistogram, 1);
  histograms_.ExpectTotalCount(kBrowserHistogram, 0);
}

TEST_F(WindowPresentationMetricsRecorderTest, IgnoresNonAppWindow) {
  std::unique_ptr<aura::Window> window =
      CreateWindowWithAppType(chromeos::AppType::NON_APP);
  WaitForPresentation();
  EXPECT_TRUE(histograms_.GetTotalCountsForPrefix(kHistogramPrefix).empty());
}

TEST_F(WindowPresentationMetricsRecorderTest, RecordsOnlyFirstShow) {
  std::unique_ptr<aura::Window> window =
      CreateWindowWithAppType(chromeos::AppType::BROWSER);
  WaitForPresentation();

  window->Hide();
  window->Show();
  WaitForPresentation();
  histograms_.ExpectTotalCount(kBrowserHistogram, 1);
}

TEST_F(WindowPresentationMetricsRecorderTest, IgnoresWindowNeverShown) {
  CreateWindowWithAppType(chromeos::AppType::BROWSER, gfx::Rect(),
                          kShellWindowId_Invalid, /*delegate=*/nullptr,
                          /*show=*/false)
      .reset();

  // Present a frame for another window so a stray sample would show up.
  std::unique_ptr<aura::Window> window =
      CreateWindowWithAppType(chromeos::AppType::ARC_APP);
  WaitForPresentation();
  histograms_.ExpectTotalCount(kBrowserHistogram, 0);
  histograms_.ExpectTotalCount(kArcAppHistogram, 1);
}

}  // namespace ash

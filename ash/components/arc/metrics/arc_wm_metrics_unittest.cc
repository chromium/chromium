// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_wm_metrics.h"

#include <memory>

#include "ash/constants/app_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace arc {

class ArcWmMetricsTest : public ash::AshTestBase {
 public:
  ArcWmMetricsTest() = default;
  ArcWmMetricsTest(const ArcWmMetricsTest&) = delete;
  ArcWmMetricsTest& operator=(const ArcWmMetricsTest&) = delete;
  ~ArcWmMetricsTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    arc_wm_metrics_ = std::make_unique<ArcWmMetrics>();
  }

  void OnWindowCloseRequested(aura::Window* window) {
    arc_wm_metrics_->OnWindowCloseRequested(window);
  }

 private:
  std::unique_ptr<ArcWmMetrics> arc_wm_metrics_;
};

TEST_F(ArcWmMetricsTest, TestWindowMaximizeDelayMetrics) {
  ash::AppType app_type = ash::AppType::ARC_APP;
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100), app_type);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetWindowMaximizedTimeHistogramName(app_type);
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->Maximize();
  histogram_tester.ExpectTotalCount(histogram_name, 1);

  // If the old window state type is not `kNormal`, the data should not be
  // recorded in histogram.
  widget->Minimize();
  widget->Maximize();
  histogram_tester.ExpectTotalCount(histogram_name, 1);

  // The histogram should not record data when maximizing in tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  widget->Maximize();
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

TEST_F(ArcWmMetricsTest, TestWindowMinimizeDelayMetrics) {
  ash::AppType app_type = ash::AppType::ARC_APP;
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100), app_type);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMinimize);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetWindowMinimizedTimeHistogramName(app_type);
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->Minimize();
  histogram_tester.ExpectTotalCount(histogram_name, 1);

  // The histogram should not record data when minimizing in tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  widget->Maximize();
  widget->Minimize();
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

TEST_F(ArcWmMetricsTest, TestWindowCloseDelayMetrics) {
  auto window =
      CreateAppWindow(gfx::Rect(0, 0, 100, 100), ash::AppType::ARC_APP);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetArcWindowClosedTimeHistogramName();
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  OnWindowCloseRequested(window.get());
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  window.reset();
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

TEST_F(ArcWmMetricsTest, TestWindowEnterTabletModeDelayMetrics) {
  ash::AppType app_type = ash::AppType::ARC_APP;
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100), app_type);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(app_type);
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  ash::TabletModeControllerTestApi().EnterTabletMode();
  histogram_tester.ExpectTotalCount(histogram_name, 1);

  // Window maximizing histogram should not record data.
  histogram_tester.ExpectTotalCount(
      ArcWmMetrics::GetWindowMaximizedTimeHistogramName(app_type), 0);
}

TEST_F(ArcWmMetricsTest, TestMultipleWindowsEnterTabletModeDelayMetrics) {
  auto lower_window =
      CreateAppWindow(gfx::Rect(0, 0, 100, 100), ash::AppType::BROWSER);
  lower_window->Show();
  auto upper_window =
      CreateAppWindow(gfx::Rect(0, 0, 100, 100), ash::AppType::ARC_APP);
  upper_window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name_lower_window =
      ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(
          ash::AppType::BROWSER);
  const auto histogram_name_upper_window =
      ArcWmMetrics::GetWindowEnterTabletModeTimeHistogramName(
          ash::AppType::ARC_APP);

  histogram_tester.ExpectTotalCount(histogram_name_lower_window, 0);
  histogram_tester.ExpectTotalCount(histogram_name_upper_window, 0);

  // Only the data of upper window should be recorded.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  histogram_tester.ExpectTotalCount(histogram_name_lower_window, 0);
  histogram_tester.ExpectTotalCount(histogram_name_upper_window, 1);
}

TEST_F(ArcWmMetricsTest, TestWindowLeaveTabletModeDelayMetrics) {
  ash::AppType app_type = ash::AppType::ARC_APP;
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100), app_type);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetWindowExitTabletModeTimeHistogramName(app_type);
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  ash::TabletModeControllerTestApi().EnterTabletMode();
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  histogram_tester.ExpectTotalCount(histogram_name, 1);

  // If the window state of a window is maximized before entering into tablet
  // mode, its window state should remains maximized after exiting tablet mode.
  // In this case, histogram should not records data.
  views::Widget::GetWidgetForNativeWindow(window.get())->Maximize();
  ash::TabletModeControllerTestApi().EnterTabletMode();
  histogram_tester.ExpectTotalCount(histogram_name, 1);
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

}  // namespace arc

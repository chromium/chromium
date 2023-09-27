// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/arc_wm_metrics.h"

#include <memory>

#include "ash/constants/app_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/wm_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

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

 private:
  std::unique_ptr<ArcWmMetrics> arc_wm_metrics_;
};

TEST_F(ArcWmMetricsTest, TestWindowMaximizeDelayMetrics) {
  ash::AppType app_type = ash::AppType::ARC_APP;
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100), app_type);
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetWindowMaximizedTimeHistogramName(app_type);
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  auto maximize_event = std::make_unique<ash::WMEvent>(ash::WM_EVENT_MAXIMIZE);
  ash::WindowState::Get(window.get())->OnWMEvent(maximize_event.get());
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

TEST_F(ArcWmMetricsTest, TestWindowMinimizeDelayMetrics) {
  ash::AppType app_type = ash::AppType::ARC_APP;
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100), app_type);
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMinimize);
  window->Show();

  base::HistogramTester histogram_tester;
  const auto histogram_name =
      ArcWmMetrics::GetWindowMinimizedTimeHistogramName(app_type);
  histogram_tester.ExpectTotalCount(histogram_name, 0);

  auto minimize_event = std::make_unique<ash::WMEvent>(ash::WM_EVENT_MINIMIZE);
  ash::WindowState::Get(window.get())->OnWMEvent(minimize_event.get());
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

}  // namespace arc

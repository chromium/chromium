// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/pointer_metrics_recorder.h"

#include <memory>

#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const char kCombinationHistogramName[] =
    "Event.DownEventCount.PerInputFormFactorDestinationCombination2";

// Test fixture for the PointerMetricsRecorder class.
class PointerMetricsRecorderTest : public AshTestBase {
 public:
  PointerMetricsRecorderTest();

  PointerMetricsRecorderTest(const PointerMetricsRecorderTest&) = delete;
  PointerMetricsRecorderTest& operator=(const PointerMetricsRecorderTest&) =
      delete;

  ~PointerMetricsRecorderTest() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  void CreateDownEvent(ui::EventPointerType pointer_type,
                       DownEventFormFactor form_factor,
                       chromeos::AppType destination);

 protected:
  // The test target.
  std::unique_ptr<PointerMetricsRecorder> pointer_metrics_recorder_;

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Where down events are dispatched to.
  std::unique_ptr<views::Widget> widget_;
};

PointerMetricsRecorderTest::PointerMetricsRecorderTest() = default;

PointerMetricsRecorderTest::~PointerMetricsRecorderTest() = default;

void PointerMetricsRecorderTest::SetUp() {
  AshTestBase::SetUp();
  pointer_metrics_recorder_ = std::make_unique<PointerMetricsRecorder>();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  widget_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
}

void PointerMetricsRecorderTest::TearDown() {
  widget_.reset();
  pointer_metrics_recorder_.reset();
  AshTestBase::TearDown();
}

void PointerMetricsRecorderTest::CreateDownEvent(
    ui::EventPointerType pointer_type,
    DownEventFormFactor form_factor,
    chromeos::AppType destination) {
  aura::Window* window = widget_->GetNativeWindow();
  CHECK(window);
  window->SetProperty(chromeos::kAppTypeKey, destination);

  if (form_factor == DownEventFormFactor::kClamshell) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  } else {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

    display::Display::Rotation rotation =
        (form_factor == DownEventFormFactor::kTabletModeLandscape)
            ? display::Display::ROTATE_0
            : display::Display::ROTATE_90;
    ScreenOrientationControllerTestApi test_api(
        Shell::Get()->screen_orientation_controller());
    // Set the screen orientation.
    test_api.SetDisplayRotation(rotation,
                                display::Display::RotationSource::ACTIVE);
  }
  if (pointer_type == ui::EventPointerType::kMouse) {
    ui::MouseEvent mouse_down(ui::EventType::kMousePressed, gfx::Point(),
                              gfx::Point(), base::TimeTicks(), 0, 0);
    ui::Event::DispatcherApi(&mouse_down).set_target(window);
    pointer_metrics_recorder_->OnMouseEvent(&mouse_down);
  } else {
    // Pen and eraser events are touch events.
    ui::TouchEvent touch_down(ui::EventType::kTouchPressed, gfx::Point(),
                              base::TimeTicks(),
                              ui::PointerDetails(pointer_type, 0));
    ui::Event::DispatcherApi(&touch_down).set_target(window);
    pointer_metrics_recorder_->OnTouchEvent(&touch_down);
  }
}

}  // namespace

// Verifies that histogram is not recorded when receiving events that are not
// down events.
TEST_F(PointerMetricsRecorderTest, NonDownEventsInAllPointerHistogram) {
  ui::MouseEvent mouse_up(ui::EventType::kMouseReleased, gfx::Point(),
                          gfx::Point(), base::TimeTicks(), 0, 0);
  pointer_metrics_recorder_->OnMouseEvent(&mouse_up);

  histogram_tester_->ExpectTotalCount(kCombinationHistogramName, 0);
}

// Verifies that down events from different combination of input type, form
// factor and destination are recorded.
TEST_F(PointerMetricsRecorderTest, DownEventPerCombination) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  CreateDownEvent(ui::EventPointerType::kMouse, DownEventFormFactor::kClamshell,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppMouseClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppMouseTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppMouseTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kPen, DownEventFormFactor::kClamshell,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kTouch, DownEventFormFactor::kClamshell,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::NON_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kNonAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kMouse, DownEventFormFactor::kClamshell,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserMouseClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserMouseTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserMouseTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kPen, DownEventFormFactor::kClamshell,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kTouch, DownEventFormFactor::kClamshell,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::BROWSER);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kBrowserStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kMouse, DownEventFormFactor::kClamshell,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppMouseClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppMouseTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppMouseTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kPen, DownEventFormFactor::kClamshell,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kTouch, DownEventFormFactor::kClamshell,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::CHROME_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kChromeAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kMouse, DownEventFormFactor::kClamshell,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppMouseClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppMouseTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppMouseTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kPen, DownEventFormFactor::kClamshell,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kTouch, DownEventFormFactor::kClamshell,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::ARC_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kArcAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kMouse, DownEventFormFactor::kClamshell,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppMouseClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppMouseTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kMouse,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppMouseTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kPen, DownEventFormFactor::kClamshell,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kPen,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppStylusTabletPortrait), 1);

  CreateDownEvent(ui::EventPointerType::kTouch, DownEventFormFactor::kClamshell,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppStylusClamshell), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModeLandscape,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppStylusTabletLandscape), 1);

  CreateDownEvent(ui::EventPointerType::kTouch,
                  DownEventFormFactor::kTabletModePortrait,
                  chromeos::AppType::CROSTINI_APP);
  histogram_tester_->ExpectBucketCount(
      kCombinationHistogramName,
      static_cast<int>(DownEventMetric2::kCrostiniAppStylusTabletPortrait), 1);

  histogram_tester_->ExpectTotalCount(kCombinationHistogramName, 45);
}

}  // namespace ash

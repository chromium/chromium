// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/pointer_metrics_recorder.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/app_types.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

int GetDestination(views::Widget* target) {
  if (!target)
    return static_cast<int>(AppType::NON_APP);

  aura::Window* window = target->GetNativeWindow();
  DCHECK(window);
  return window->GetProperty(aura::client::kAppType);
}

DownEventMetric2 FindCombination(int destination,
                                 DownEventSource input_type,
                                 DownEventFormFactor form_factor) {
  constexpr int kNumCombinationPerDestination =
      static_cast<int>(DownEventSource::kSourceCount) *
      static_cast<int>(DownEventFormFactor::kFormFactorCount);
  int result = destination * kNumCombinationPerDestination +
               static_cast<int>(DownEventFormFactor::kFormFactorCount) *
                   static_cast<int>(input_type) +
               static_cast<int>(form_factor);
  DCHECK(result >= 0 &&
         result <= static_cast<int>(DownEventMetric2::kMaxValue));
  return static_cast<DownEventMetric2>(result);
}

void RecordUMA(ui::EventPointerType type, ui::EventTarget* event_target) {
  DCHECK_NE(type, ui::EventPointerType::POINTER_TYPE_UNKNOWN);
  views::Widget* target = views::Widget::GetTopLevelWidgetForNativeView(
      static_cast<aura::Window*>(event_target));
  DownEventFormFactor form_factor = DownEventFormFactor::kClamshell;
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    OrientationLockType screen_orientation =
        Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
    if (screen_orientation == OrientationLockType::kLandscapePrimary ||
        screen_orientation == OrientationLockType::kLandscapeSecondary) {
      form_factor = DownEventFormFactor::kTabletModeLandscape;
    } else {
      form_factor = DownEventFormFactor::kTabletModePortrait;
    }
  }

  DownEventSource input_type = DownEventSource::kUnknown;
  switch (type) {
    case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
      return;
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      input_type = DownEventSource::kMouse;
      break;
    case ui::EventPointerType::POINTER_TYPE_PEN:
      input_type = DownEventSource::kStylus;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      input_type = DownEventSource::kTouch;
      break;
    case ui::EventPointerType::POINTER_TYPE_ERASER:
      input_type = DownEventSource::kStylus;
      break;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Event.DownEventCount.PerInputFormFactorDestinationCombination2",
      FindCombination(GetDestination(target), input_type, form_factor));
}

}  // namespace

PointerMetricsRecorder::PointerMetricsRecorder() {
  Shell::Get()->AddPreTargetHandler(this);
}

PointerMetricsRecorder::~PointerMetricsRecorder() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void PointerMetricsRecorder::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    RecordUMA(event->pointer_details().pointer_type, event->target());
}

void PointerMetricsRecorder::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    RecordUMA(event->pointer_details().pointer_type, event->target());
}

}  // namespace ash

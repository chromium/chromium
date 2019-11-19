// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/touch_calibrator_controller.h"

#include <algorithm>
#include <memory>

#include "ash/display/touch_calibrator_view.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ash {
namespace {

void InitInternalTouchDeviceIds(std::set<int>& internal_touch_device_ids) {
  internal_touch_device_ids.clear();
  const std::vector<ui::TouchscreenDevice>& device_list =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  for (const auto& touchscreen_device : device_list) {
    if (touchscreen_device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      internal_touch_device_ids.insert(touchscreen_device.id);
  }
}

// Returns a transform to undo any transformations that are applied to events
// originating from the touch device identified with |touch_device_id|. This
// transform converts the event's location to the raw touch location.
gfx::Transform CalculateEventTransformer(int touch_device_id) {
  const display::DisplayManager* display_manager =
      Shell::Get()->display_manager();
  const std::vector<ui::TouchscreenDevice>& device_list =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();

  auto device_it = std::find_if(
      device_list.begin(), device_list.end(),
      [&](const auto& device) { return device.id == touch_device_id; });
  DCHECK(device_it != device_list.end())
      << "Device id " << touch_device_id
      << " is invalid. No such device connected to system";

  int64_t previous_display_id =
      display_manager->touch_device_manager()->GetAssociatedDisplay(
          display::TouchDeviceIdentifier::FromDevice(*device_it));

  // If the touch device is not associated with any display. This may happen in
  // tests when the test does not setup the |ui::TouchDeviceTransform| before
  // generating a touch event.
  if (previous_display_id == display::kInvalidDisplayId)
    return gfx::Transform();

  // Undo the event transformations that the previous display applied on the
  // event location. We want to store the raw event location information.
  gfx::Transform tm =
      Shell::Get()
          ->window_tree_host_manager()
          ->GetAshWindowTreeHostForDisplayId(previous_display_id)
          ->AsWindowTreeHost()
          ->GetRootTransform();
  return tm;
}

}  // namespace

// Time interval after a touch event during which all other touch events are
// ignored during calibration.
const base::TimeDelta TouchCalibratorController::kTouchIntervalThreshold =
    base::TimeDelta::FromMilliseconds(200);

TouchCalibratorController::TouchCalibratorController()
    : last_touch_timestamp_(base::Time::Now()) {}

TouchCalibratorController::~TouchCalibratorController() {
  touch_calibrator_views_.clear();
  StopCalibrationAndResetParams();
}

void TouchCalibratorController::OnDisplayConfigurationChanged() {
  touch_calibrator_views_.clear();
  StopCalibrationAndResetParams();
}

void TouchCalibratorController::StartCalibration(
    const display::Display& target_display,
    bool is_custom_calibration,
    TouchCalibrationCallback opt_callback) {
  state_ = is_custom_calibration ? CalibrationState::kCustomCalibration
                                 : CalibrationState::kNativeCalibration;

  if (opt_callback)
    opt_callback_ = std::move(opt_callback);

  target_display_ = target_display;

  // Clear all touch calibrator views used in any previous calibration.
  touch_calibrator_views_.clear();

  // Set the touch device id as invalid so it can be set during calibration.
  touch_device_id_ = ui::InputDevice::kInvalidId;

  // Populate |internal_touch_device_ids_| with the ids of touch devices that
  // are currently associated with the internal display and are of type
  // |ui::InputDeviceType::INPUT_DEVICE_INTERNAL|.
  InitInternalTouchDeviceIds(internal_touch_device_ids_);

  // If this is a native touch calibration, then initialize the UX for it.
  if (state_ == CalibrationState::kNativeCalibration) {
    Shell::Get()->window_tree_host_manager()->AddObserver(this);

    // Reset the calibration data.
    touch_point_quad_.fill(std::make_pair(gfx::Point(0, 0), gfx::Point(0, 0)));

    std::vector<display::Display> displays =
        display::Screen::GetScreen()->GetAllDisplays();

    for (const display::Display& display : displays) {
      bool is_primary_view = display.id() == target_display_.id();
      touch_calibrator_views_[display.id()] =
          std::make_unique<TouchCalibratorView>(display, is_primary_view);
    }
  }

  Shell::Get()->touch_transformer_controller()->SetForCalibration(true);

  // Add self as an event handler target.
  Shell::Get()->AddPreTargetHandler(this);
}

void TouchCalibratorController::StopCalibrationAndResetParams() {
  if (!IsCalibrating())
    return;
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);

  Shell::Get()->touch_transformer_controller()->SetForCalibration(false);

  // Remove self as the event handler.
  Shell::Get()->RemovePreTargetHandler(this);

  // Transition all touch calibrator views to their final state for a graceful
  // exit if this is touch calibration with native UX.
  if (state_ == CalibrationState::kNativeCalibration) {
    for (const auto& it : touch_calibrator_views_)
      it.second->SkipToFinalState();
  }

  state_ = CalibrationState::kInactive;

  if (opt_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(opt_callback_), false /* failure */));
    opt_callback_.Reset();
  }
}

void TouchCalibratorController::CompleteCalibration(
    const CalibrationPointPairQuad& pairs,
    const gfx::Size& display_size) {
  bool did_find_touch_device = false;
  display::TouchDeviceIdentifier touch_device_identifier =
      display::TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier();

  const std::vector<ui::TouchscreenDevice>& device_list =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  for (const auto& device : device_list) {
    if (device.id == touch_device_id_) {
      touch_device_identifier =
          display::TouchDeviceIdentifier::FromDevice(device);
      did_find_touch_device = true;
      break;
    }
  }

  if (!did_find_touch_device) {
    VLOG(1) << "No touch device with id: " << touch_device_id_ << " found to "
            << "complete touch calibration for display with id: "
            << target_display_.id() << ". Storing it as a fallback";
  } else if (touch_device_identifier ==
             display::TouchDeviceIdentifier::
                 GetFallbackTouchDeviceIdentifier()) {
    LOG(ERROR)
        << "Hash collision in generating touch device identifier for "
        << " device. Hash Generated: " << touch_device_identifier
        << " || Fallback touch device identifier: "
        << display::TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier();
  }

  if (opt_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(opt_callback_), true /* success */));
    opt_callback_.Reset();
  }
  StopCalibrationAndResetParams();
  Shell::Get()->display_manager()->SetTouchCalibrationData(
      target_display_.id(), pairs, display_size, touch_device_identifier);
}

bool TouchCalibratorController::IsCalibrating() const {
  return state_ != CalibrationState::kInactive;
}

// ui::EventHandler:
void TouchCalibratorController::OnKeyEvent(ui::KeyEvent* key) {
  if (state_ != CalibrationState::kNativeCalibration)
    return;
  // Detect ESC key press.
  if (key->type() == ui::ET_KEY_PRESSED && key->key_code() == ui::VKEY_ESCAPE)
    StopCalibrationAndResetParams();

  key->StopPropagation();
}

void TouchCalibratorController::OnTouchEvent(ui::TouchEvent* touch) {
  if (!IsCalibrating())
    return;
  if (touch->type() != ui::ET_TOUCH_RELEASED)
    return;
  if (base::Time::Now() - last_touch_timestamp_ < kTouchIntervalThreshold)
    return;
  last_touch_timestamp_ = base::Time::Now();

  // If the touch event originated from a touch device that is associated with
  // the internal display, then ignore it.
  if (internal_touch_device_ids_.count(touch->source_device_id()))
    return;

  if (touch_device_id_ == ui::InputDevice::kInvalidId) {
    touch_device_id_ = touch->source_device_id();
    event_transformer_ = CalculateEventTransformer(touch_device_id_);
  }

  // If this is a custom touch calibration, then everything else is managed
  // by the application responsible for the custom calibration UX.
  if (state_ == CalibrationState::kCustomCalibration)
    return;
  touch->StopPropagation();

  TouchCalibratorView* target_screen_calibration_view =
      touch_calibrator_views_[target_display_.id()].get();

  // If this is the final state, then store all calibration data and stop
  // calibration.
  if (target_screen_calibration_view->state() ==
      TouchCalibratorView::CALIBRATION_COMPLETE) {
    gfx::RectF calibration_bounds(
        target_screen_calibration_view->GetLocalBounds());
    Shell::Get()
        ->window_tree_host_manager()
        ->GetAshWindowTreeHostForDisplayId(target_display_.id())
        ->AsWindowTreeHost()
        ->GetRootTransform()
        .TransformRect(&calibration_bounds);
    CompleteCalibration(touch_point_quad_,
                        gfx::ToRoundedSize(calibration_bounds.size()));
    return;
  }

  int state_index;
  // Maps the state to an integer value. Assigns a non negative integral value
  // for a state in which the user can interact with the the interface.
  switch (target_screen_calibration_view->state()) {
    case TouchCalibratorView::DISPLAY_POINT_1:
      state_index = 0;
      break;
    case TouchCalibratorView::DISPLAY_POINT_2:
      state_index = 1;
      break;
    case TouchCalibratorView::DISPLAY_POINT_3:
      state_index = 2;
      break;
    case TouchCalibratorView::DISPLAY_POINT_4:
      state_index = 3;
      break;
    default:
      // Return early if the interface is in a state that does not allow user
      // interaction.
      return;
  }

  // Store touch point corresponding to its display point.
  gfx::Point display_point;
  if (target_screen_calibration_view->GetDisplayPointLocation(&display_point)) {
    // If the screen has a root transform applied, the display point does not
    // correctly map to the touch point. This is specially evident if the
    // display is rotated or a device scale factor is applied. The display point
    // needs to have the root transform applied as well to correctly pair it
    // with the touch point.
    Shell::Get()
        ->window_tree_host_manager()
        ->GetAshWindowTreeHostForDisplayId(target_display_.id())
        ->AsWindowTreeHost()
        ->GetRootTransform()
        .TransformPoint(&display_point);

    // Why do we need this? To understand this we need to know the life of an
    // event location. The event location undergoes the following
    // transformations along its path from the device to the event handler that
    // is this class.
    //
    // Touch Device -> EventFactoryEvdev -> DrmWindowHost
    //     -> WindowEventDispatcher -> EventHandler(this)
    //
    //  - The touch device dispatches the raw device event location. Lets assume
    //    this is (x, y).
    //  - The EventFactoryEvdev applies a touch transform that includes the
    //    calibration information as well as an offset of the native bounds
    //    of the display. This effectively converts the coordinates of the event
    //    from the raw device event location to the native screen coordinates.
    //    It gets the offset information from DrmWindowHost via the
    //    ManagedDisplayInfo class. If the offset of the PlatformWindow is (A,B)
    //    then the event location after this stage would be (x + A, y + B).
    //  - The DrmWindowHost removes the offset from the event location so that
    //    the location becomes relative to the platform window's origin. In
    //    Chrome OS it so happens that each display is its own platform window.
    //    So an offset equal to the display's origin in screen space is
    //    subtracted from the event location. This effectively undoes the
    //    previous step's transformation. Thus the event location after this
    //    step is (x, y) again.
    //  - WindowEventDispatcher applies an inverse root transform on the event
    //    location. This means that if the display is rotated or has a device
    //    scale factor, then those transformation are also applied to the event
    //    location. It effectively converts the coordinates from platform window
    //    coordinates to the aura's root window coordinates. The display in
    //    context here is the display that is associated with the touch device
    //    from which the event originated from.
    //
    // Up until the output of DrmWindowHost, everything is as expected. But
    // WindowEventDispatcher applies an inverse root transform which modifies
    // the raw event location that we wanted. Moreover, it modifies the raw
    // event location using the root transform of the display that the touch
    // device was previously associated with. To solve this, we need to undo the
    // changes made to the event location by WindowEventDispatcher. This is what
    // is achieved by |event_transformer_|.
    gfx::PointF event_location_f(touch->location_f());
    event_transformer_.TransformPoint(&event_location_f);

    touch_point_quad_[state_index] =
        std::make_pair(display_point, gfx::ToRoundedPoint(event_location_f));
  } else {
    // TODO(malaykeshav): Display some kind of error for the user.
    NOTREACHED() << "Touch calibration failed. Could not retrieve location for"
                    " display point. Retry calibration.";
  }

  target_screen_calibration_view->AdvanceToNextState();
}

}  // namespace ash

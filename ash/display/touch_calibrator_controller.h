// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_TOUCH_CALIBRATOR_CONTROLLER_H_
#define ASH_DISPLAY_TOUCH_CALIBRATOR_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ui {
class KeyEvent;
class TouchEvent;
}  // namespace ui

namespace ash {

class TouchCalibratorView;

// TouchCalibratorController is responsible for managing the touch calibration
// process. In case of native touch calibration it is also responsible for
// collecting the touch calibration associated data from the user. It
// instantiates TouchCalibratorView classes to present the native UX interface
// the user can interact with for calibration.
// This controller ensures that only one instance of calibration is running at
// any given time.
class ASH_EXPORT TouchCalibratorController
    : public ui::EventHandler,
      public display::DisplayManagerObserver {
 public:
  using CalibrationPointPairQuad =
      display::TouchCalibrationData::CalibrationPointPairQuad;
  using TouchCalibrationCallback = base::OnceCallback<void(bool)>;

  static const base::TimeDelta kTouchIntervalThreshold;

  TouchCalibratorController();

  TouchCalibratorController(const TouchCalibratorController&) = delete;
  TouchCalibratorController& operator=(const TouchCalibratorController&) =
      delete;

  ~TouchCalibratorController() override;

  // ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // display::DisplayManagerObserver
  void OnDidApplyDisplayChanges() override;

  // Starts the calibration process for the given |target_display|.
  // |opt_callback| is an optional callback that if provided is executed
  // with the success or failure of the calibration as a boolean argument.
  void StartCalibration(const display::Display& target_display,
                        bool is_custom_calibration,
                        TouchCalibrationCallback opt_callback);

  // Maps all monitors to their matching touchscreen device. Provided callback
  // will be called once all displays have been mapped.
  void StartNativeTouchscreenMappingExperience(
      TouchCalibrationCallback opt_callback);

  // Stops any ongoing calibration process. This is a hard stop which does not
  // save any calibration data. Call CompleteCalibration() if you wish to save
  // calibration data.
  void StopCalibrationAndResetParams();

  // Completes the touch calibration by storing the calibration data for the
  // display.
  void CompleteCalibration(const CalibrationPointPairQuad& pairs,
                           const gfx::Size& display_size);

  // Returns true if any type of touch calibration is active.
  bool IsCalibrating() const;

 private:
  friend class TouchCalibratorControllerTest;
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest, TouchThreshold);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest, CustomCalibration);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest,
                           CustomCalibrationInvalidTouchId);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest,
                           InternalTouchDeviceIsRejected);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest,
                           Mapping_TwoExternalDisplays_FullFlow);
  FRIEND_TEST_ALL_PREFIXES(TouchCalibratorControllerTest,
                           Mapping_TwoExternalDisplays_SkipFirst);

  enum class CalibrationState {
    // Indicates that the touch calibration is currently active with the built
    // in native UX.
    kNativeCalibration = 0,

    // Indicates that the touch calibration is currently active with the built
    // in native UX for all displays.
    kNativeCalibrationTouchscreenMapping,

    // Indicates that the touch calibration is currently active with a custom
    // UX via the extensions API.
    kCustomCalibration,

    // Indicates that touch calibration is currently inactive.
    kInactive
  };

  // Iterates over to run the calibration experience on the next display.
  // Used when running the touchscreen mapping experience.
  void CalibrateNextDisplay();

  CalibrationState state_ = CalibrationState::kInactive;

  // A map for TouchCalibrator view with the key as display id of the display
  // it is present in.
  std::map<int64_t, views::UniqueWidgetPtr> touch_calibrator_widgets_;

  // The display which is being calibrated by the touch calibrator controller.
  // This is valid only if |is_calibrating| is set to true.
  display::Display target_display_;

  // During calibration this stores the timestamp when the previous touch event
  // was received.
  base::Time last_touch_timestamp_;

  // This is populated during calibration, based on the source id of the device
  // the events are originating from.
  int touch_device_id_ = ui::InputDevice::kInvalidId;

  // A set of ids that belong to touch devices associated with the internal
  // display and are of type |ui::InputDeviceType::INPUT_DEVICE_INTERNAL|. This
  // is only valid when |state_| is not |kInactive|.
  std::set<int> internal_touch_device_ids_;

  // An array of Calibration point pairs. This stores all the 4 display and
  // touch input point pairs that will be used for calibration.
  CalibrationPointPairQuad touch_point_quad_;

  // A callback to be called when touch calibration completes when started via
  // `StartCalibration`.
  TouchCalibrationCallback opt_callback_;
  // A callback to be called when the native touch mapping experience has
  // completed. This is started via `StartNativeTouchscreenMappingExperience`.
  TouchCalibrationCallback opt_callback_all_displays_;

  // The list of displays were already mapped to touchscreen devices in the
  // current instantiation of the touchscreen mapping experience.
  base::flat_set<int64_t> already_mapped_display_ids_;

  // The touch device under calibration may be re-associated to another display
  // during calibration. In such a case, the events originating from the touch
  // device are tranformed based on parameters of the previous display it was
  // linked to. We need to undo these transformations before recording the event
  // locations.
  gfx::Transform event_transformer_;
};

}  // namespace ash
#endif  // ASH_DISPLAY_TOUCH_CALIBRATOR_CONTROLLER_H_

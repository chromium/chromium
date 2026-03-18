// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_
#define ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/observer_list_types.h"
#include "base/types/optional_ref.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

class OverscanCalibrator;
class TouchCalibratorController;

// Describes how the displays are laid out.
enum class DisplayLayoutMode {
  // In normal mode displays are laid out as described by
  // DisplayLayoutInfo::layouts.
  kNormal,
  // In unified desktop mode, a single desktop will be stretched across all
  // available displays.
  kUnified,
  // In mirrored mode, the display defined by DisplayLayoutInfo.mirrorSourceId
  // will be mirrored in the displays defined by
  // DisplayLayoutInfo::mirrorDestinationIds, or in all other displays if
  // mirrorDestinationIds is empty.
  kMirrored
};

// Defines the layout mode and details.
struct ASH_EXPORT DisplayLayoutInfo {
  DisplayLayoutInfo();
  DisplayLayoutInfo(const DisplayLayoutInfo& other);
  DisplayLayoutInfo(DisplayLayoutInfo&& other) noexcept;
  DisplayLayoutInfo& operator=(const DisplayLayoutInfo& other);
  DisplayLayoutInfo& operator=(DisplayLayoutInfo&& other) noexcept;
  ~DisplayLayoutInfo();

  // The layout mode to use, see DisplayLayoutMode for details.
  DisplayLayoutMode layout_mode;

  // Ignored if layout_mode is not kMirrored. Otherwise, if provided, specifies
  // the unique identifier of the source display for mirroring. If not provided,
  // mirror_destination_ids will be ignored and default ('normal') mirrored mode
  // will be enabled.
  std::optional<int64_t> mirror_source_id;

  // Ignored if layout_mode is not kMirrored. Otherwise, if provided, specifies
  // the unique identifiers of the displays to mirror the source display. If
  // empty, all displays will mirror the source display.
  std::optional<std::vector<int64_t>> mirror_destination_ids;

  // An array of layouts describing a directed graph of displays. Required if
  // layout_mode is kNormal or kMirrored and not all displays are mirrored
  // ('mixed' mode). Ignored if layout_mode is kUnified.
  std::optional<std::vector<display::DisplayPlacement>> layouts;
};

// Properties for configuring an individual display, used in
// SetDisplayProperties.
struct ASH_EXPORT DisplayConfigProperties {
  DisplayConfigProperties();
  DisplayConfigProperties(const DisplayConfigProperties& other) = delete;
  DisplayConfigProperties(DisplayConfigProperties&& other) noexcept;
  DisplayConfigProperties& operator=(const DisplayConfigProperties& other) =
      delete;
  DisplayConfigProperties& operator=(DisplayConfigProperties&& other) noexcept;
  ~DisplayConfigProperties();

  // If true, makes the display primary. No-op if set to false.
  bool set_primary = false;

  // If provided, sets the display's overscan insets to the provided value.
  // Note: overscan values may not be negative or larger than a half of the
  // screen's size. Overscan cannot be changed on the internal monitor.
  std::optional<gfx::Insets> overscan;

  // If provided, updates the display's rotation, or if the auto-rotation is
  // allowed in the device, it can be used to set or clear the user rotation
  // lock, enabling or disabling auto-rotation.
  std::optional<crosapi::mojom::DisplayRotationOptions> rotation;

  // If provided, updates the display's logical bounds origin. Note: when
  // updating the display origin, some constraints will be applied. so the final
  // bounds origin may be different than the one set. The actual bounds will be
  // reflected in DisplayUnitInfo. Cannot be changed on the primary display (or
  // if set_primary is true).
  std::optional<gfx::Point> bounds_origin;

  // If non-zero, updates the zoom associated with the display. This zoom
  // performs relayout and repaint thus resulting in a better quality zoom than
  // just performing a pixel by pixel stretch enlargement.
  double display_zoom_factor = 0.0;

  // Optional DisplayMode properties to set. This should match one of the
  // modes listed in DisplayUnitInfo.available_display_modes. Other custom
  // modes may or may not be valid.
  crosapi::mojom::DisplayModePtr display_mode;
};

// SetDisplayLayoutInfo or SetDisplayProperties result.
enum class DisplayConfigResult {
  // Operation was successful.
  kSuccess,
  // Operation is not supported.
  kInvalidOperationError,
  // Input display ID represents an invalid display.
  kInvalidDisplayIdError,
  // Unified desktop mode is disabled.
  kUnifiedNotEnabledError,
  // Input property for operation is out of range. E.g. display zoom factor,
  // bounds origin or overscan.
  kPropertyValueOutOfRangeError,
  // Operation is not supported for internal displays.
  kNotSupportedOnInternalDisplayError,
  // Negative values are not supported for the operation.
  kNegativeValueError,
  // Setting the display mode failed.
  kSetDisplayModeError,
  // Invalid display layout error.
  kInvalidDisplayLayoutError,
  // Mode requires multiple displays.
  kSingleDisplayError,
  // Mirror mode source ID is invalid.
  kMirrorModeSourceIdError,
  // Mirror mode destination ID is invalid.
  kMirrorModeDestIdError,
  // Calibration is not available (e.g. no external touch screen device).
  kCalibrationNotAvailableError,
  // Calibration was not started.
  kCalibrationNotStartedError,
  // Touch calibration is already active.
  kCalibrationInProgressError,
  // Invalid input data for calibration.
  kCalibrationInvalidDataError,
  // Calibration procedure failed.
  kCalibrationFailedError,
};

// Interface for configuring displays in Chrome OS.
class CrosDisplayConfig {
 public:
  using TouchCalibrationCallback =
      base::OnceCallback<void(DisplayConfigResult)>;

  class Observer : public base::CheckedObserver {
   public:
    // Called when the display layout or any display properties change.
    virtual void OnDisplayConfigChanged() = 0;
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the display layout info, including the list of layouts.
  virtual DisplayLayoutInfo GetDisplayLayoutInfo() = 0;

  // Sets the layout mode, mirroring, and layouts. Returns kSuccess if the
  // layout is valid or an error value otherwise.
  virtual DisplayConfigResult SetDisplayLayoutInfo(
      const DisplayLayoutInfo& info) = 0;

  // Returns the properties for all displays. If |single_unified| is true, a
  // single display will be returned if the display layout is in unified mode.
  virtual std::vector<crosapi::mojom::DisplayUnitInfoPtr>
  GetDisplayUnitInfoList(bool single_unified) = 0;

  // Sets |properties| for individual display with identifier |id|. |source|
  // should describe who initiated the change. Returns Success if the properties
  // are valid or an error value otherwise.
  virtual DisplayConfigResult SetDisplayProperties(
      const std::string& id,
      const DisplayConfigProperties& properties,
      crosapi::mojom::DisplayConfigSource source) = 0;

  // Enables or disables unified desktop mode. If the current display mode is
  // kMirrored the mode will not be changed, if it is kNormal then the mode will
  // be set to kUnified.
  virtual void SetUnifiedDesktopEnabled(bool enabled) = 0;

  // Starts, updates, completes, or resets overscan calibration for the display
  // with identifier |display_id|. If |op| is kAdjust, |delta| describes the
  // amount to change the overscan value.
  virtual DisplayConfigResult OverscanCalibration(
      const std::string& display_id,
      crosapi::mojom::DisplayConfigOperation op,
      const std::optional<gfx::Insets>& delta) = 0;

  // Starts, completes, or resets touch calibration for the display with
  // identifier |display_id|. If |op| is kShowNative shows the native
  // calibration UI. Runs the callback after performing the operation or on
  // error.
  virtual void TouchCalibration(
      const std::string& display_id,
      crosapi::mojom::DisplayConfigOperation op,
      base::optional_ref<const display::TouchCalibrationData> calibration,
      TouchCalibrationCallback callback) = 0;

  // Sets |id| of display to render identification highlight on. Invalid |id|
  // turns identification highlight off.
  virtual void HighlightDisplay(int64_t display_id) = 0;

  // Updates preview indicators with change in position of display being dragged
  // in display layouts section of the display settings page. |display_id| is
  // the ID of the display being dragged. |delta_x| and |delta_y| are the change
  // in position of the dragged display since DragDisplayDelta() was last
  // called. |display_id| remains the same while the drag is in progress, once
  // the display is dropped, the new layout is applied, updating the display
  // configuration.
  virtual void DragDisplayDelta(int64_t display_id,
                                int32_t delta_x,
                                int32_t delta_y) = 0;
};

class ASH_EXPORT CrosDisplayConfigImpl final : public CrosDisplayConfig {
 public:
  CrosDisplayConfigImpl();

  CrosDisplayConfigImpl(const CrosDisplayConfigImpl&) = delete;
  CrosDisplayConfigImpl& operator=(const CrosDisplayConfigImpl&) = delete;

  ~CrosDisplayConfigImpl();

  // CrosDisplayConfig:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  DisplayLayoutInfo GetDisplayLayoutInfo() override;
  DisplayConfigResult SetDisplayLayoutInfo(
      const DisplayLayoutInfo& info) override;
  std::vector<crosapi::mojom::DisplayUnitInfoPtr> GetDisplayUnitInfoList(
      bool single_unified) override;
  DisplayConfigResult SetDisplayProperties(
      const std::string& id,
      const DisplayConfigProperties& properties,
      crosapi::mojom::DisplayConfigSource source) override;
  void SetUnifiedDesktopEnabled(bool enabled) override;
  DisplayConfigResult OverscanCalibration(
      const std::string& display_id,
      crosapi::mojom::DisplayConfigOperation op,
      const std::optional<gfx::Insets>& delta) override;
  void TouchCalibration(
      const std::string& display_id,
      crosapi::mojom::DisplayConfigOperation op,
      base::optional_ref<const display::TouchCalibrationData> calibration,
      TouchCalibrationCallback callback) override;
  void HighlightDisplay(int64_t display_id) override;
  void DragDisplayDelta(int64_t display_id,
                        int32_t delta_x,
                        int32_t delta_y) override;

  bool IsCalibrating() const;

  TouchCalibratorController* touch_calibrator_for_test() {
    return touch_calibrator_.get();
  }

 private:
  class ObserverImpl;
  friend class OverscanCalibratorTest;

  OverscanCalibrator* GetOverscanCalibrator(const std::string& id);

  std::unique_ptr<ObserverImpl> observer_impl_;
  std::map<std::string, std::unique_ptr<OverscanCalibrator>>
      overscan_calibrators_;
  std::unique_ptr<TouchCalibratorController> touch_calibrator_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_

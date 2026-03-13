// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_
#define ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"

namespace ash {

class OverscanCalibrator;
class TouchCalibratorController;

// Interface for configuring displays in Chrome OS.
class CrosDisplayConfig {
 public:
  // TODO(crbug.com/485123493): Get rid of callbacks where possible.
  using GetDisplayLayoutInfoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayLayoutInfoPtr)>;
  using GetDisplayLayoutInfoMojoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayLayoutInfoPtr)>;
  using SetDisplayLayoutInfoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using SetDisplayLayoutInfoMojoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using GetDisplayUnitInfoListCallback =
      base::OnceCallback<void(std::vector<crosapi::mojom::DisplayUnitInfoPtr>)>;
  using GetDisplayUnitInfoListMojoCallback =
      base::OnceCallback<void(std::vector<crosapi::mojom::DisplayUnitInfoPtr>)>;
  using SetDisplayPropertiesCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using SetDisplayPropertiesMojoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using OverscanCalibrationCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using OverscanCalibrationMojoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using TouchCalibrationCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;
  using TouchCalibrationMojoCallback =
      base::OnceCallback<void(crosapi::mojom::DisplayConfigResult)>;

  // Observers are notified when the display layout or any display properties
  // change.
  virtual void AddObserver(
      crosapi::mojom::CrosDisplayConfigObserver* observer) = 0;
  virtual void RemoveObserver(
      crosapi::mojom::CrosDisplayConfigObserver* observer) = 0;

  // Returns the display layout info, including the list of layouts.
  virtual void GetDisplayLayoutInfo(GetDisplayLayoutInfoCallback callback) = 0;

  // Sets the layout mode, mirroring, and layouts. Returns kSuccess if the
  // layout is valid or an error value otherwise.
  virtual void SetDisplayLayoutInfo(crosapi::mojom::DisplayLayoutInfoPtr info,
                                    SetDisplayLayoutInfoCallback callback) = 0;

  // Returns the properties for all displays. If |single_unified| is true, a
  // single display will be returned if the display layout is in unified mode.
  virtual void GetDisplayUnitInfoList(
      bool single_unified,
      GetDisplayUnitInfoListCallback callback) = 0;

  // Sets |properties| for individual display with identifier |id|. |source|
  // should describe who initiated the change. Returns Success if the properties
  // are valid or an error value otherwise.
  virtual void SetDisplayProperties(
      const std::string& id,
      crosapi::mojom::DisplayConfigPropertiesPtr properties,
      crosapi::mojom::DisplayConfigSource source,
      SetDisplayPropertiesCallback callback) = 0;

  // Enables or disables unified desktop mode. If the current display mode is
  // kMirrored the mode will not be changed, if it is kNormal then the mode will
  // be set to kUnified.
  virtual void SetUnifiedDesktopEnabled(bool enabled) = 0;

  // Starts, updates, completes, or resets overscan calibration for the display
  // with identifier |display_id|. If |op| is kAdjust, |delta| describes the
  // amount to change the overscan value. Runs the callback after performing the
  // operation or on error.
  virtual void OverscanCalibration(const std::string& display_id,
                                   crosapi::mojom::DisplayConfigOperation op,
                                   const std::optional<gfx::Insets>& delta,
                                   OverscanCalibrationCallback callback) = 0;

  // Starts, completes, or resets touch calibration for the display with
  // identifier |display_id|. If |op| is kShowNative shows the native
  // calibration UI. Runs the callback after performing the operation or on
  // error.
  virtual void TouchCalibration(const std::string& display_id,
                                crosapi::mojom::DisplayConfigOperation op,
                                crosapi::mojom::TouchCalibrationPtr calibration,
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
  void AddObserver(
      crosapi::mojom::CrosDisplayConfigObserver* observer) override;
  void RemoveObserver(
      crosapi::mojom::CrosDisplayConfigObserver* observer) override;
  void GetDisplayLayoutInfo(GetDisplayLayoutInfoCallback callback) override;
  void SetDisplayLayoutInfo(crosapi::mojom::DisplayLayoutInfoPtr info,
                            SetDisplayLayoutInfoCallback callback) override;
  void GetDisplayUnitInfoList(bool single_unified,
                              GetDisplayUnitInfoListCallback callback) override;
  void SetDisplayProperties(
      const std::string& id,
      crosapi::mojom::DisplayConfigPropertiesPtr properties,
      crosapi::mojom::DisplayConfigSource source,
      SetDisplayPropertiesCallback callback) override;
  void SetUnifiedDesktopEnabled(bool enabled) override;
  void OverscanCalibration(const std::string& display_id,
                           crosapi::mojom::DisplayConfigOperation op,
                           const std::optional<gfx::Insets>& delta,
                           OverscanCalibrationCallback callback) override;
  void TouchCalibration(const std::string& display_id,
                        crosapi::mojom::DisplayConfigOperation op,
                        crosapi::mojom::TouchCalibrationPtr calibration,
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

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
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

class OverscanCalibrator;
class TouchCalibratorController;

// ASH_EXPORT for use in chrome unit_tests for DisplayInfoProviderChromeOS.
class ASH_EXPORT CrosDisplayConfig
    : public crosapi::mojom::CrosDisplayConfigController {
 public:
  CrosDisplayConfig();

  CrosDisplayConfig(const CrosDisplayConfig&) = delete;
  CrosDisplayConfig& operator=(const CrosDisplayConfig&) = delete;

  ~CrosDisplayConfig() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::CrosDisplayConfigController>
          receiver);

  // crosapi::mojom::CrosDisplayConfigController:
  void AddObserver(
      mojo::PendingAssociatedRemote<crosapi::mojom::CrosDisplayConfigObserver>
          observer) override;
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

  TouchCalibratorController* touch_calibrator_for_test() {
    return touch_calibrator_.get();
  }

 private:
  class ObserverImpl;
  friend class OverscanCalibratorTest;

  OverscanCalibrator* GetOverscanCalibrator(const std::string& id);

  std::unique_ptr<ObserverImpl> observer_impl_;
  mojo::ReceiverSet<crosapi::mojom::CrosDisplayConfigController> receivers_;
  std::map<std::string, std::unique_ptr<OverscanCalibrator>>
      overscan_calibrators_;
  std::unique_ptr<TouchCalibratorController> touch_calibrator_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_

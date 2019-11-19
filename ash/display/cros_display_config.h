// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_
#define ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_

#include <map>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

class OverscanCalibrator;
class TouchCalibratorController;

// ASH_EXPORT for use in chrome unit_tests for DisplayInfoProviderChromeOS.
class ASH_EXPORT CrosDisplayConfig : public mojom::CrosDisplayConfigController {
 public:
  CrosDisplayConfig();
  ~CrosDisplayConfig() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver);

  // mojom::CrosDisplayConfigController:
  void AddObserver(
      mojo::PendingAssociatedRemote<mojom::CrosDisplayConfigObserver> observer)
      override;
  void GetDisplayLayoutInfo(GetDisplayLayoutInfoCallback callback) override;
  void SetDisplayLayoutInfo(mojom::DisplayLayoutInfoPtr info,
                            SetDisplayLayoutInfoCallback callback) override;
  void GetDisplayUnitInfoList(bool single_unified,
                              GetDisplayUnitInfoListCallback callback) override;
  void SetDisplayProperties(const std::string& id,
                            mojom::DisplayConfigPropertiesPtr properties,
                            mojom::DisplayConfigSource source,
                            SetDisplayPropertiesCallback callback) override;
  void SetUnifiedDesktopEnabled(bool enabled) override;
  void OverscanCalibration(const std::string& display_id,
                           mojom::DisplayConfigOperation op,
                           const base::Optional<gfx::Insets>& delta,
                           OverscanCalibrationCallback callback) override;
  void TouchCalibration(const std::string& display_id,
                        mojom::DisplayConfigOperation op,
                        mojom::TouchCalibrationPtr calibration,
                        TouchCalibrationCallback callback) override;

  TouchCalibratorController* touch_calibrator_for_test() {
    return touch_calibrator_.get();
  }

 private:
  class ObserverImpl;
  friend class OverscanCalibratorTest;

  OverscanCalibrator* GetOverscanCalibrator(const std::string& id);

  std::unique_ptr<ObserverImpl> observer_impl_;
  mojo::ReceiverSet<mojom::CrosDisplayConfigController> receivers_;
  std::map<std::string, std::unique_ptr<OverscanCalibrator>>
      overscan_calibrators_;
  std::unique_ptr<TouchCalibratorController> touch_calibrator_;

  DISALLOW_COPY_AND_ASSIGN(CrosDisplayConfig);
};

}  // namespace ash

#endif  // ASH_DISPLAY_CROS_DISPLAY_CONFIG_H_

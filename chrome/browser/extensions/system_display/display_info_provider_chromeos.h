// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

#include <memory>

#include "ash/display/cros_display_config.h"
#include "ash/shell_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "extensions/browser/display_info_provider_base.h"
#include "ui/display/manager/touch_device_manager.h"

namespace ash {
class Shell;
}  // namespace ash

namespace extensions {

class DisplayInfoProviderChromeOS : public DisplayInfoProviderBase,
                                    public ash::ShellObserver,
                                    public ash::CrosDisplayConfig::Observer {
 public:
  DisplayInfoProviderChromeOS();

  DisplayInfoProviderChromeOS(const DisplayInfoProviderChromeOS&) = delete;
  DisplayInfoProviderChromeOS& operator=(const DisplayInfoProviderChromeOS&) =
      delete;

  ~DisplayInfoProviderChromeOS() override;

  // DisplayInfoProvider implementation.
  void SetDisplayProperties(
      const std::string& display_id,
      const api::system_display::DisplayProperties& properties,
      ErrorCallback callback) override;
  base::expected<void, std::string> SetDisplayLayout(
      const DisplayLayoutList& layouts) override;
  void EnableUnifiedDesktop(bool enable) override;
  void GetAllDisplaysInfo(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList result)> callback) override;
  DisplayLayoutList GetDisplayLayout() override;
  bool OverscanCalibrationStart(const std::string& id) override;
  bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta) override;
  bool OverscanCalibrationReset(const std::string& id) override;
  bool OverscanCalibrationComplete(const std::string& id) override;
  void ShowNativeTouchCalibration(const std::string& id,
                                  ErrorCallback callback) override;
  bool StartCustomTouchCalibration(const std::string& id) override;
  bool CompleteCustomTouchCalibration(
      const api::system_display::TouchCalibrationPairQuad& pairs,
      const api::system_display::Bounds& bounds) override;
  bool ClearTouchCalibration(const std::string& id) override;
  void SetMirrorMode(const api::system_display::MirrorModeInfo& info,
                     ErrorCallback callback) override;
  void StartObserving() override;
  void StopObserving() override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  // ash::CrosDisplayConfig::Observer
  void OnDisplayConfigChanged() override;

 private:
  void CallTouchCalibration(
      const std::string& id,
      crosapi::mojom::DisplayConfigOperation op,
      base::optional_ref<const display::TouchCalibrationData> calibration,
      ErrorCallback callback);

  raw_ptr<ash::CrosDisplayConfig> cros_display_config_;
  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
  base::ScopedObservation<ash::CrosDisplayConfig,
                          ash::CrosDisplayConfig::Observer>
      cros_display_config_observation_{this};
  std::string touch_calibration_target_id_;
  base::WeakPtrFactory<DisplayInfoProviderChromeOS> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

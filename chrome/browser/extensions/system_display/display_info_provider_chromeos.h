// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

#include <map>
#include <memory>

#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace service_manager {
class Connector;
}

namespace extensions {

class DisplayInfoProviderChromeOS
    : public DisplayInfoProvider,
      public ash::mojom::CrosDisplayConfigObserver {
 public:
  explicit DisplayInfoProviderChromeOS(service_manager::Connector* connector);
  ~DisplayInfoProviderChromeOS() override;

  // DisplayInfoProvider implementation.
  void SetDisplayProperties(
      const std::string& display_id,
      const api::system_display::DisplayProperties& properties,
      ErrorCallback callback) override;
  void SetDisplayLayout(const DisplayLayoutList& layouts,
                        ErrorCallback callback) override;
  void EnableUnifiedDesktop(bool enable) override;
  void GetAllDisplaysInfo(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList result)> callback) override;
  void GetDisplayLayout(
      base::OnceCallback<void(DisplayLayoutList result)> callback) override;
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

  // ash::mojom::CrosDisplayConfigObserver
  void OnDisplayConfigChanged() override;

 private:
  void CallSetDisplayLayoutInfo(ash::mojom::DisplayLayoutInfoPtr layout_info,
                                ErrorCallback callback,
                                ash::mojom::DisplayLayoutInfoPtr cur_info);
  void CallGetDisplayUnitInfoList(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList result)> callback,
      ash::mojom::DisplayLayoutInfoPtr layout);
  void CallTouchCalibration(const std::string& id,
                            ash::mojom::DisplayConfigOperation op,
                            ash::mojom::TouchCalibrationPtr calibration,
                            ErrorCallback callback);

  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config_;
  mojo::AssociatedReceiver<ash::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_receiver_{this};
  std::string touch_calibration_target_id_;
  base::WeakPtrFactory<DisplayInfoProviderChromeOS> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProviderChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

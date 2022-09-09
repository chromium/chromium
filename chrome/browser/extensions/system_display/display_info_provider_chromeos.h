// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {

class DisplayInfoProviderChromeOS
    : public DisplayInfoProvider,
      public crosapi::mojom::CrosDisplayConfigObserver {
 public:
  explicit DisplayInfoProviderChromeOS(
      mojo::PendingRemote<crosapi::mojom::CrosDisplayConfigController>
          display_config);

  DisplayInfoProviderChromeOS(const DisplayInfoProviderChromeOS&) = delete;
  DisplayInfoProviderChromeOS& operator=(const DisplayInfoProviderChromeOS&) =
      delete;

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

  // crosapi::mojom::CrosDisplayConfigObserver
  void OnDisplayConfigChanged() override;

 private:
  void CallSetDisplayLayoutInfo(
      crosapi::mojom::DisplayLayoutInfoPtr layout_info,
      ErrorCallback callback,
      crosapi::mojom::DisplayLayoutInfoPtr cur_info);
  void CallGetDisplayUnitInfoList(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList result)> callback,
      crosapi::mojom::DisplayLayoutInfoPtr layout);
  void OnGetDisplayUnitInfoList(
      crosapi::mojom::DisplayLayoutInfoPtr layout,
      base::OnceCallback<void(DisplayUnitInfoList)> callback,
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list);
  void CallTouchCalibration(const std::string& id,
                            crosapi::mojom::DisplayConfigOperation op,
                            crosapi::mojom::TouchCalibrationPtr calibration,
                            ErrorCallback callback);

  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;
  mojo::AssociatedReceiver<crosapi::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_receiver_{this};
  std::string touch_calibration_target_id_;
  base::WeakPtrFactory<DisplayInfoProviderChromeOS> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_CHROMEOS_H_

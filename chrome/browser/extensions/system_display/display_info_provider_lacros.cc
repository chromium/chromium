// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_lacros.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/extensions/system_display/display_info_provider_utils.h"
#include "chrome/browser/extensions/system_display/system_display_serialization.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/common/api/system_display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

DisplayInfoProviderLacros::DisplayInfoProviderLacros() {
  // Relies on the fact that the instance is a singleton managed by
  // DisplayInfoProvider::Get(), and assumes that instantiation takes place
  // after LacrosService has been initialized.
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);
  if (lacros_service->IsAvailable<crosapi::mojom::SystemDisplay>() &&
      lacros_service->GetInterfaceVersion(
          crosapi::mojom::SystemDisplay::Uuid_) >=
          static_cast<int>(crosapi::mojom::SystemDisplay::
                               kAddDisplayChangeObserverMinVersion)) {
    lacros_service->GetRemote<crosapi::mojom::SystemDisplay>()
        ->AddDisplayChangeObserver(
            receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

DisplayInfoProviderLacros::~DisplayInfoProviderLacros() = default;

void DisplayInfoProviderLacros::GetAllDisplaysInfo(
    bool single_unified,
    base::OnceCallback<void(DisplayUnitInfoList)> callback) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::SystemDisplay>()) {
    auto cb =
        base::BindOnce(&DisplayInfoProviderLacros::OnCrosapiResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    lacros_service->GetRemote<crosapi::mojom::SystemDisplay>()
        ->GetDisplayUnitInfoList(single_unified, std::move(cb));

  } else {
    std::move(callback).Run(DisplayUnitInfoList());
  }
}

void DisplayInfoProviderLacros::OnCrosapiResult(
    base::OnceCallback<void(DisplayUnitInfoList)> callback,
    std::vector<crosapi::mojom::SysDisplayUnitInfoPtr> src_info_list) {
  DisplayUnitInfoList dst_info_list(src_info_list.size());
  for (size_t i = 0; i < src_info_list.size(); ++i) {
    DCHECK(src_info_list[i]);
    extensions::api::system_display::DeserializeDisplayUnitInfo(
        *src_info_list[i], &dst_info_list[i]);
  }
  std::move(callback).Run(std::move(dst_info_list));
}

void DisplayInfoProviderLacros::OnCrosapiDisplayChanged() {
  DispatchOnDisplayChangedEvent();
}

void DisplayInfoProviderLacros::GetDisplayLayout(
    base::OnceCallback<void(DisplayLayoutList)> callback) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service
          ->IsAvailable<crosapi::mojom::CrosDisplayConfigController>()) {
    auto& remote =
        lacros_service
            ->GetRemote<crosapi::mojom::CrosDisplayConfigController>();

    remote->GetDisplayLayoutInfo(
        base::BindOnce(&OnGetDisplayLayoutResult, std::move(callback)));

  } else {
    LOG(ERROR) << "Cros display config service is not available.";
    std::move(callback).Run(DisplayLayoutList());
  }
}

void DisplayInfoProviderLacros::SetDisplayProperties(
    const std::string& display_id,
    const api::system_display::DisplayProperties& properties,
    ErrorCallback callback) {
  constexpr char kTempError[] =
      "SetDisplayProperties to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  std::move(callback).Run(kTempError);
}

void DisplayInfoProviderLacros::SetDisplayLayout(
    const DisplayLayoutList& layouts,
    ErrorCallback callback) {
  constexpr char kTempError[] = "SetDisplayLayout to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  std::move(callback).Run(kTempError);
}

void DisplayInfoProviderLacros::EnableUnifiedDesktop(bool enable) {
  constexpr char kTempError[] =
      "EnableUnifiedDesktop to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
}

bool DisplayInfoProviderLacros::OverscanCalibrationStart(
    const std::string& id) {
  constexpr char kTempError[] =
      "OverscanCalibrationStart to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}
bool DisplayInfoProviderLacros::OverscanCalibrationAdjust(
    const std::string& id,
    const api::system_display::Insets& delta) {
  constexpr char kTempError[] =
      "OverscanCalibrationAdjust to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}

bool DisplayInfoProviderLacros::OverscanCalibrationReset(
    const std::string& id) {
  constexpr char kTempError[] =
      "OverscanCalibrationAdjust to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}

bool DisplayInfoProviderLacros::OverscanCalibrationComplete(
    const std::string& id) {
  constexpr char kTempError[] =
      "OverscanCalibrationComplete to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}

void DisplayInfoProviderLacros::ShowNativeTouchCalibration(
    const std::string& id,
    ErrorCallback callback) {
  constexpr char kTempError[] =
      "ShowNativeTouchCalibration to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  std::move(callback).Run(kTempError);
}

bool DisplayInfoProviderLacros::StartCustomTouchCalibration(
    const std::string& id) {
  constexpr char kTempError[] =
      "StartCustomTouchCalibration to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}

bool DisplayInfoProviderLacros::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds) {
  constexpr char kTempError[] =
      "CompleteCustomTouchCalibration to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}

bool DisplayInfoProviderLacros::ClearTouchCalibration(const std::string& id) {
  constexpr char kTempError[] =
      "ClearTouchCalibration to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  return false;
}

void DisplayInfoProviderLacros::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  constexpr char kTempError[] = "SetMirrorMode to be implemented in Lacros";
  NOTIMPLEMENTED() << kTempError;
  std::move(callback).Run(kTempError);
}

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderLacros>();
}

}  // namespace extensions

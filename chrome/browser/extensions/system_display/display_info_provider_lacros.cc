// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_lacros.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/extensions/system_display/system_display_serialization.h"
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
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> src_info_list) {
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

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderLacros>();
}

}  // namespace extensions

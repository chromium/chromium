// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/system_display_ash.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/extensions/system_display/system_display_serialization.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace crosapi {

SystemDisplayAsh::SystemDisplayAsh() = default;

SystemDisplayAsh::~SystemDisplayAsh() = default;

void SystemDisplayAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SystemDisplay> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SystemDisplayAsh::GetDisplayUnitInfoList(
    bool single_unified,
    GetDisplayUnitInfoListCallback callback) {
  extensions::DisplayInfoProvider* provider =
      extensions::DisplayInfoProvider::Get();
  provider->GetAllDisplaysInfo(
      single_unified,
      base::BindOnce(&SystemDisplayAsh::OnDisplayInfoResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemDisplayAsh::OnDisplayInfoResult(
    GetDisplayUnitInfoListCallback callback,
    std::vector<DisplayUnitInfo> src_info_list) {
  std::vector<crosapi::mojom::DisplayUnitInfoPtr> dst_info_list;
  for (const auto& src_info : src_info_list) {
    dst_info_list.emplace_back(
        extensions::api::system_display::SerializeDisplayUnitInfo(src_info));
  }
  std::move(callback).Run(std::move(std::move(dst_info_list)));
}

}  // namespace crosapi

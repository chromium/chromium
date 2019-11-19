// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/default_scale_factor_retriever.h"

#include <utility>

#include "ash/public/mojom/constants.mojom.h"
#include "base/bind.h"

namespace ash {

DefaultScaleFactorRetriever::DefaultScaleFactorRetriever() {}

void DefaultScaleFactorRetriever::Start(
    mojo::PendingRemote<ash::mojom::CrosDisplayConfigController>
        cros_display_config) {
  cros_display_config_.Bind(std::move(cros_display_config));
  auto callback = base::BindOnce(
      &DefaultScaleFactorRetriever::OnDefaultScaleFactorRetrieved,
      weak_ptr_factory_.GetWeakPtr());
  cros_display_config_->GetDisplayUnitInfoList(
      /*single_unified=*/false,
      base::BindOnce(
          [](GetDefaultScaleFactorCallback callback,
             std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
            // TODO(oshima): This does not return correct value in docked
            // mode.
            for (const ash::mojom::DisplayUnitInfoPtr& info : info_list) {
              if (info->is_internal) {
                DCHECK(info->available_display_modes.size());
                std::move(callback).Run(
                    info->available_display_modes[0]->device_scale_factor);
                return;
              }
            }
            std::move(callback).Run(1.0f);
          },
          std::move(callback)));
}

DefaultScaleFactorRetriever::~DefaultScaleFactorRetriever() = default;

void DefaultScaleFactorRetriever::GetDefaultScaleFactor(
    GetDefaultScaleFactorCallback callback) {
  if (display::Display::HasForceDeviceScaleFactor()) {
    return std::move(callback).Run(
        display::Display::GetForcedDeviceScaleFactor());
  }
  if (default_scale_factor_ > 0) {
    std::move(callback).Run(default_scale_factor_);
    return;
  }
  callback_ = std::move(callback);
}

void DefaultScaleFactorRetriever::CancelCallback() {
  callback_.Reset();
}

void DefaultScaleFactorRetriever::SetDefaultScaleFactorForTest(
    float scale_factor) {
  default_scale_factor_ = scale_factor;
}

void DefaultScaleFactorRetriever::OnDefaultScaleFactorRetrieved(
    float scale_factor) {
  DCHECK_GT(scale_factor, 0.f);
  default_scale_factor_ = scale_factor;
  if (!callback_.is_null())
    std::move(callback_).Run(scale_factor);
}

}  // namespace ash

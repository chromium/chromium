// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_customization_id.h"

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace ash {

namespace {

void OnMachineStatisticsReady(
    base::OnceCallback<void(std::optional<std::string_view>)>
        get_customization_id_callback) {
  std::move(get_customization_id_callback)
      .Run(system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kCustomizationIdKey));
}

}  // namespace

void GetCustomizationId(
    base::OnceCallback<void(std::optional<std::string_view>)>
        get_customization_id_callback) {
  system::StatisticsProvider::GetInstance()->ScheduleOnMachineStatisticsLoaded(
      base::BindOnce(&OnMachineStatisticsReady,
                     std::move(get_customization_id_callback)));
}

}  // namespace ash

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/metrics_ash.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace crosapi {

MetricsAsh::MetricsAsh() : weak_ptr_factory_(this) {}
MetricsAsh::~MetricsAsh() = default;

void MetricsAsh::BindReceiver(mojo::PendingReceiver<mojom::Metrics> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MetricsAsh::GetFullHardwareClass(GetFullHardwareClassCallback callback) {
  if (full_hardware_class_) {
    std::move(callback).Run(full_hardware_class_.value());
    return;
  }

  callbacks_.push_back(std::move(callback));
  if (callbacks_.size() == 1) {
    ash::system::StatisticsProvider::GetInstance()
        ->ScheduleOnMachineStatisticsLoaded(
            base::BindOnce(&MetricsAsh::OnMachineStatisticsLoaded,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void MetricsAsh::OnMachineStatisticsLoaded() {
  const std::optional<std::string_view> full_hardware_class =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          "hardware_class");
  if (full_hardware_class) {
    full_hardware_class_ = std::string(full_hardware_class.value());
  } else {
    full_hardware_class_ = "";
  }
  std::vector<GetFullHardwareClassCallback> callbacks;
  callbacks.swap(callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(full_hardware_class_.value());
  }
}

}  // namespace crosapi

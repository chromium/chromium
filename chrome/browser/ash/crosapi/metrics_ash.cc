// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/metrics_ash.h"

#include <utility>

#include "base/bind.h"
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
    chromeos::system::StatisticsProvider::GetInstance()
        ->ScheduleOnMachineStatisticsLoaded(
            base::BindOnce(&MetricsAsh::OnMachineStatisticsLoaded,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void MetricsAsh::OnMachineStatisticsLoaded() {
  const absl::optional<base::StringPiece> full_hardware_class =
      chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
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

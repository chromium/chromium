// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"

namespace ash::quick_start {

// TODO impl

TargetDeviceConnectionBroker::TargetDeviceConnectionBroker() = default;
TargetDeviceConnectionBroker::~TargetDeviceConnectionBroker() = default;

void TargetDeviceConnectionBroker::GetFeatureSupportStatusAsync(
    FeatureSupportStatusCallback callback) {
  feature_status_callbacks_.push_back(std::move(callback));
  MaybeNotifyFeatureStatus();
}

void TargetDeviceConnectionBroker::MaybeNotifyFeatureStatus() {
  FeatureSupportStatus status = GetFeatureSupportStatus();
  if (status == FeatureSupportStatus::kUndetermined)
    return;

  auto callbacks = std::exchange(feature_status_callbacks_, {});

  for (auto& callback : callbacks) {
    std::move(callback).Run(status);
  }
}

}  // namespace ash::quick_start

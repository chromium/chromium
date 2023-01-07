// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_iio_sensor_instance.h"

namespace arc {

FakeIioSensorInstance::FakeIioSensorInstance() = default;
FakeIioSensorInstance::~FakeIioSensorInstance() = default;

void FakeIioSensorInstance::Init(
    mojo::PendingRemote<mojom::IioSensorHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakeIioSensorInstance::OnTabletModeChanged(bool is_tablet_mode_on) {
  is_tablet_mode_on_ = is_tablet_mode_on;
}

}  // namespace arc

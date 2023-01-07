// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_IIO_SENSOR_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_IIO_SENSOR_INSTANCE_H_

#include "ash/components/arc/mojom/iio_sensor.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeIioSensorInstance : public mojom::IioSensorInstance {
 public:
  FakeIioSensorInstance();
  ~FakeIioSensorInstance() override;
  FakeIioSensorInstance(const FakeIioSensorInstance&) = delete;
  FakeIioSensorInstance& operator=(const FakeIioSensorInstance&) = delete;

  bool is_tablet_mode_on() const { return is_tablet_mode_on_; }

  // mojom::IioSensorInstance overrides:
  void Init(mojo::PendingRemote<mojom::IioSensorHost> host_remote,
            InitCallback callback) override;
  void OnTabletModeChanged(bool is_tablet_mode_on) override;

 private:
  mojo::Remote<mojom::IioSensorHost> host_remote_;

  bool is_tablet_mode_on_ = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_IIO_SENSOR_INSTANCE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_
#define ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_

#include "ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace device_sync {

// Fake DeviceSyncObserver implementation for tests.
class FakeDeviceSyncObserver
    : public ash::device_sync::mojom::DeviceSyncObserver {
 public:
  FakeDeviceSyncObserver();

  FakeDeviceSyncObserver(const FakeDeviceSyncObserver&) = delete;
  FakeDeviceSyncObserver& operator=(const FakeDeviceSyncObserver&) = delete;

  ~FakeDeviceSyncObserver() override;

  mojo::PendingRemote<ash::device_sync::mojom::DeviceSyncObserver>
  GenerateRemote();

  size_t num_enrollment_events() { return num_enrollment_events_; }
  size_t num_sync_events() { return num_sync_events_; }

  // ash::device_sync::mojom::DeviceSyncObserver:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

 private:
  size_t num_enrollment_events_ = 0u;
  size_t num_sync_events_ = 0u;

  mojo::ReceiverSet<ash::device_sync::mojom::DeviceSyncObserver> receivers_;
};

}  // namespace device_sync

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash::device_sync {
using ::chromeos::device_sync::FakeDeviceSyncObserver;
}

#endif  // ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_OBSERVER_H_

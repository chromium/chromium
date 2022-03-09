// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_
#define ASH_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_

namespace chromeos {

namespace device_sync {

// Creates a stub DeviceSync factory that initializes a stub DeviceSync, then
// sets that factory as the DeviceSyncImpl custom factory.
void SetStubDeviceSyncFactory();

}  // namespace device_sync

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace device_sync {
using ::chromeos::device_sync::SetStubDeviceSyncFactory;
}
}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_

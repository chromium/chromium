// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_

#include "ash/services/secure_channel/device_id_pair.h"
#include "ash/services/secure_channel/error_tolerant_ble_advertisement.h"
#include "base/callback.h"
#include "base/unguessable_token.h"

namespace ash::secure_channel {

// Test double for ErrorTolerantBleAdvertisement.
class FakeErrorTolerantBleAdvertisement : public ErrorTolerantBleAdvertisement {
 public:
  FakeErrorTolerantBleAdvertisement(
      const DeviceIdPair& device_id_pair,
      base::OnceCallback<void(const DeviceIdPair&)> destructor_callback);

  FakeErrorTolerantBleAdvertisement(const FakeErrorTolerantBleAdvertisement&) =
      delete;
  FakeErrorTolerantBleAdvertisement& operator=(
      const FakeErrorTolerantBleAdvertisement&) = delete;

  ~FakeErrorTolerantBleAdvertisement() override;

  const base::UnguessableToken& id() const { return id_; }

  void InvokeStopCallback();

  // ErrorTolerantBleAdvertisement:
  void Stop(base::OnceClosure callback) override;
  bool HasBeenStopped() override;

 private:
  base::UnguessableToken id_;
  base::OnceCallback<void(const DeviceIdPair&)> destructor_callback_;
  base::OnceClosure stop_callback_;
  bool stopped_ = false;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_

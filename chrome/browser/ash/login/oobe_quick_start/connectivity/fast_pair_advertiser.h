// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAST_PAIR_ADVERTISER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAST_PAIR_ADVERTISER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace ash::quick_start {

class AdvertisingId;

// FastPairAdvertiser broadcasts advertisements with the service UUID
// 0xFE2C and model ID 0x41C0D9. When the remote device detects this
// advertisement it will trigger a prompt to begin Quick Start.
class FastPairAdvertiser : public device::BluetoothAdvertisement::Observer {
 public:
  class Factory {
   public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    static std::unique_ptr<FastPairAdvertiser> Create(
        scoped_refptr<device::BluetoothAdapter> adapter);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<FastPairAdvertiser> CreateInstance(
        scoped_refptr<device::BluetoothAdapter> adapter) = 0;

   private:
    static Factory* factory_instance_;
  };

  explicit FastPairAdvertiser(scoped_refptr<device::BluetoothAdapter> adapter);
  ~FastPairAdvertiser() override;
  FastPairAdvertiser(const FastPairAdvertiser&) = delete;
  FastPairAdvertiser& operator=(const FastPairAdvertiser&) = delete;

  // Begin broadcasting Fast Pair advertisement.
  virtual void StartAdvertising(base::OnceClosure callback,
                                base::OnceClosure error_callback,
                                const AdvertisingId& advertising_id,
                                bool use_pin_authentication);

  // Stop broadcasting Fast Pair advertisement.
  virtual void StopAdvertising(base::OnceClosure callback);

 private:
  friend class FastPairAdvertiserTest;

  // device::BluetoothAdvertisement::Observer:
  void AdvertisementReleased(
      device::BluetoothAdvertisement* advertisement) override;

  void RegisterAdvertisement(base::OnceClosure callback,
                             base::OnceClosure error_callback,
                             const AdvertisingId& advertising_id);
  void OnRegisterAdvertisement(
      base::OnceClosure callback,
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  void OnRegisterAdvertisementError(
      base::OnceClosure error_callback,
      device::BluetoothAdvertisement::ErrorCode error_code);
  void UnregisterAdvertisement(base::OnceClosure callback);
  void OnUnregisterAdvertisement();
  void OnUnregisterAdvertisementError(
      device::BluetoothAdvertisement::ErrorCode error_code);

  // Returns metadata in format [ advertising_id (16 bytes) ].
  std::vector<uint8_t> GenerateManufacturerMetadata(
      const AdvertisingId& advertising_id);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisement_;
  base::OnceClosure stop_callback_;
  // Timer to keep track of advertising duration.
  std::unique_ptr<base::ElapsedTimer> advertising_timer_;
  // Used for metrics to record advertising method.
  quick_start_metrics::AdvertisingMethod advertising_method_;
  base::WeakPtrFactory<FastPairAdvertiser> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAST_PAIR_ADVERTISER_H_

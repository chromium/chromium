// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

// FastInitiationManager broadcasts advertisements with the service UUID
// 0xFE2C. The broadcast data will be
// 0xFC128E along with 2 additional bytes of metadata at the end. Some remote
// devices background scan for Fast Initiation advertisements, as a signal to
// begin advertising via Nearby Connections.
class FastInitiationManager : public device::BluetoothAdvertisement::Observer {
 public:
  enum class FastInitType : uint8_t {
    kNotify = 0,
    kSilent = 1,
  };

  class Factory {
   public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    static std::unique_ptr<FastInitiationManager> Create(
        scoped_refptr<device::BluetoothAdapter> adapter);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<FastInitiationManager> CreateInstance(
        scoped_refptr<device::BluetoothAdapter> adapter) = 0;

   private:
    static Factory* factory_instance_;
  };

  explicit FastInitiationManager(
      scoped_refptr<device::BluetoothAdapter> adapter);
  ~FastInitiationManager() override;
  FastInitiationManager(const FastInitiationManager&) = delete;
  FastInitiationManager& operator=(const FastInitiationManager&) = delete;

  // Begin broadcasting Fast Initiation advertisement.
  virtual void StartAdvertising(FastInitType type,
                                base::OnceClosure callback,
                                base::OnceClosure error_callback);

  // Stop broadcasting Fast Initiation advertisement.
  virtual void StopAdvertising(base::OnceClosure callback);

 private:
  // device::BluetoothAdvertisement::Observer:
  void AdvertisementReleased(
      device::BluetoothAdvertisement* advertisement) override;

  void RegisterAdvertisement(FastInitType type,
                             base::OnceClosure callback,
                             base::OnceClosure error_callback);
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

  // Fast Init V1 metadata has 2 bytes, in format
  // [ version (3 bits) | type (3 bits) | uwb_enable (1 bit) | reserved (1 bit),
  // adjusted_tx_power (1 byte) ].
  std::vector<uint8_t> GenerateFastInitV1Metadata(FastInitType type);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<device::BluetoothAdvertisement> advertisement_;
  base::OnceClosure stop_callback_;
  base::WeakPtrFactory<FastInitiationManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_MANAGER_H_

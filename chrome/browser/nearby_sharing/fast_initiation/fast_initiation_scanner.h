// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_FAST_INITIATION_SCANNER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_FAST_INITIATION_SCANNER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"

class FastInitiationScanner
    : public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  FastInitiationScanner(scoped_refptr<device::BluetoothAdapter> adapter,
                        base::RepeatingClosure device_found_callback,
                        base::RepeatingClosure device_lost_callback,
                        base::OnceClosure scanner_invalidated_callback);
  ~FastInitiationScanner() override;
  FastInitiationScanner(const FastInitiationScanner&) = delete;
  FastInitiationScanner& operator=(const FastInitiationScanner&) = delete;

  bool AreFastInitiationDevicesDetected() const;

 private:
  void StartScanning();

  // device::BluetoothLowEnergyScanSession::Delegate:
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      absl::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override;
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override;
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::RepeatingClosure device_found_callback_;
  base::RepeatingClosure device_lost_callback_;
  base::OnceClosure scanner_invalidated_callback_;
  std::unique_ptr<device::BluetoothLowEnergyScanSession>
      background_scan_session_;
  // Set of remote devices that we detect are currently emitting fast initiation
  // advertisements.
  base::flat_set<std::string> devices_attempting_to_share_;

  base::WeakPtrFactory<FastInitiationScanner> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_FAST_INITIATION_SCANNER_H_

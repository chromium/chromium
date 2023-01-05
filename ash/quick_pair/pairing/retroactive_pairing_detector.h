// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_H_
#define ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/observer_list_types.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

class Device;

// A RetroactivePairingDetector instance is responsible for detecting Fast Pair
// devices that can be paired retroactively, and notifying observers of this
// device.
class RetroactivePairingDetector {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnRetroactivePairFound(scoped_refptr<Device> device) = 0;
  };

  virtual ~RetroactivePairingDetector() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_H_

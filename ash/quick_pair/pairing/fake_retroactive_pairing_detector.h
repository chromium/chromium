// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAKE_RETROACTIVE_PAIRING_DETECTOR_H_
#define ASH_QUICK_PAIR_PAIRING_FAKE_RETROACTIVE_PAIRING_DETECTOR_H_

#include <optional>

#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "base/observer_list.h"

namespace ash {
namespace quick_pair {

class Device;

class FakeRetroactivePairingDetector : public RetroactivePairingDetector {
 public:
  FakeRetroactivePairingDetector();
  FakeRetroactivePairingDetector(const FakeRetroactivePairingDetector&) =
      delete;
  FakeRetroactivePairingDetector& operator=(
      const FakeRetroactivePairingDetector&) = delete;
  ~FakeRetroactivePairingDetector() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyRetroactivePairFound(scoped_refptr<Device> device);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAKE_RETROACTIVE_PAIRING_DETECTOR_H_

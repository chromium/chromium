// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fake_retroactive_pairing_detector.h"

#include "ash/quick_pair/common/device.h"

namespace ash {
namespace quick_pair {

FakeRetroactivePairingDetector::FakeRetroactivePairingDetector() = default;

FakeRetroactivePairingDetector::~FakeRetroactivePairingDetector() = default;

void FakeRetroactivePairingDetector::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeRetroactivePairingDetector::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeRetroactivePairingDetector::NotifyRetroactivePairFound(
    scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnRetroactivePairFound(device);
}

}  // namespace quick_pair
}  // namespace ash

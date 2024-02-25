// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/companion_app/mock_companion_app_broker.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

MockCompanionAppBroker::MockCompanionAppBroker() = default;

MockCompanionAppBroker::~MockCompanionAppBroker() = default;

void MockCompanionAppBroker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockCompanionAppBroker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace quick_pair
}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_keyed_service.h"

#include <memory>

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

namespace ash {
namespace quick_pair {

QuickPairKeyedService::QuickPairKeyedService(std::unique_ptr<Mediator> mediator)
    : mediator_(std::move(mediator)) {}

QuickPairKeyedService::~QuickPairKeyedService() = default;

void QuickPairKeyedService::Shutdown() {
  mediator_.reset();
}

}  // namespace quick_pair
}  // namespace ash

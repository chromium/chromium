// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_FAKE_QUICK_PAIR_MEDIATOR_FACTORY_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_FAKE_QUICK_PAIR_MEDIATOR_FACTORY_H_

#include <memory>

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

namespace ash::quick_pair {

// TODO(jackshira): Make this more of a builder and provide setters for the
// different components.
class FakeQuickPairMediatorFactory : public Mediator::Factory {
 private:
  // Mediator::Factory:
  std::unique_ptr<Mediator> BuildInstance() override;
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_FAKE_QUICK_PAIR_MEDIATOR_FACTORY_H_

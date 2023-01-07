// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_KEYED_SERVICE_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_KEYED_SERVICE_H_

#include <memory>

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
namespace quick_pair {

// This class is intentionally 'thin' (i.e. holds references to other classes)
// to decouple the logic from the KeyedService base class and ease testing.
class QuickPairKeyedService : public KeyedService {
  explicit QuickPairKeyedService(std::unique_ptr<Mediator> mediator);
  QuickPairKeyedService(const QuickPairKeyedService&) = delete;
  QuickPairKeyedService& operator=(const QuickPairKeyedService&) = delete;
  ~QuickPairKeyedService() override;

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<Mediator> mediator_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_QUICK_PAIR_KEYED_SERVICE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGINATION_MODEL_TRANSITION_WAITER_H_
#define ASH_APP_LIST_VIEWS_PAGINATION_MODEL_TRANSITION_WAITER_H_

#include <memory>

#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace base {
class RunLoop;
}

namespace ash {

// Waiter that can be used in tests to wait for pagination transitions in a
// pagination model end.
class PaginationModelTransitionWaiter : public PaginationModelObserver {
 public:
  explicit PaginationModelTransitionWaiter(PaginationModel* model);
  ~PaginationModelTransitionWaiter() override;

  PaginationModelTransitionWaiter(const PaginationModelTransitionWaiter&) =
      delete;
  PaginationModelTransitionWaiter& operator=(
      const PaginationModelTransitionWaiter&) = delete;

  void Wait();

 private:
  // PaginationModelObserver:
  void TransitionEnded() override;

  std::unique_ptr<base::RunLoop> run_loop_;
  const raw_ptr<PaginationModel> pagination_model_;
  base::ScopedObservation<PaginationModel, PaginationModelObserver>
      scoped_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGINATION_MODEL_TRANSITION_WAITER_H_

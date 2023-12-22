// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_MEMORY_PRESSURE_EVALUATOR_H_
#define CHROME_BROWSER_LACROS_LACROS_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/resource_manager.mojom.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/reclaim_target.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"
#include "mojo/public/cpp/bindings/receiver.h"

// LacrosMemoryPressureEvaluator handles the observation of our free memory. It
// notifies the MemoryPressureListener of memory fill level changes, so that it
// can take action to reduce memory resources accordingly.
class LacrosMemoryPressureEvaluator
    : public memory_pressure::SystemMemoryPressureEvaluator,
      public crosapi::mojom::MemoryPressureObserver {
 public:
  explicit LacrosMemoryPressureEvaluator(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);
  ~LacrosMemoryPressureEvaluator() override;

  LacrosMemoryPressureEvaluator(const LacrosMemoryPressureEvaluator&) = delete;
  LacrosMemoryPressureEvaluator& operator=(
      const LacrosMemoryPressureEvaluator&) = delete;

  // Returns the current system memory pressure evaluator.
  static LacrosMemoryPressureEvaluator* Get();

  // Returns the cached amount of memory to reclaim.
  // TODO: Lacros tab manager delegate will use this value to determine how many
  // tabs to discard.
  memory_pressure::ReclaimTarget GetCachedReclaimTarget();

  // Implements mojom::MemoryPressureObserver.
  void MemoryPressure(crosapi::mojom::MemoryPressurePtr pressure) override;

 private:
  bool ShouldNotify(
      const base::MemoryPressureListener::MemoryPressureLevel old_vote,
      const base::MemoryPressureListener::MemoryPressureLevel current_vote);

  memory_pressure::ReclaimTarget cached_reclaim_target_;

  // We keep track of how long it has been since we last notified at the
  // moderate level. It's initialized to a null value to indicate there is no
  // previous moderate notification.
  base::TimeTicks last_moderate_notification_;

  mojo::Receiver<crosapi::mojom::MemoryPressureObserver> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LacrosMemoryPressureEvaluator> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_MEMORY_PRESSURE_EVALUATOR_H_

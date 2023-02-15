// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_memory_pressure_evaluator.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Processes PressureCallback calls by just storing the sequence of events so we
// can validate that we received the expected pressure levels as the test runs.
void PressureCallback(
    std::vector<base::MemoryPressureListener::MemoryPressureLevel>* history,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  history->push_back(level);
}

}  // namespace

TEST(LacrosMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  // We will use a mock listener to keep track of our kernel notifications which
  // cause event to be fired. We can just examine the sequence of pressure
  // events when we're done to validate that the pressure events were as
  // expected.
  std::vector<base::MemoryPressureListener::MemoryPressureLevel>
      pressure_events;
  auto listener = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&PressureCallback, &pressure_events));

  memory_pressure::MultiSourceMemoryPressureMonitor monitor;

  auto evaluator =
      std::make_unique<LacrosMemoryPressureEvaluator>(monitor.CreateVoter());

  // At this point we have no memory pressure.
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Moderate Pressure.
  crosapi::mojom::MemoryPressurePtr pressure =
      crosapi::mojom::MemoryPressure::New();
  pressure->level = crosapi::mojom::MemoryPressureLevel::kModerate;
  pressure->reclaim_target_kb = 1000;
  evaluator->MemoryPressure(pressure->Clone());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Critical Pressure.
  pressure->level = crosapi::mojom::MemoryPressureLevel::kCritical;
  evaluator->MemoryPressure(pressure->Clone());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator->current_vote());

  // Moderate Pressure.
  pressure->level = crosapi::mojom::MemoryPressureLevel::kModerate;
  evaluator->MemoryPressure(pressure->Clone());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // No pressure, note: this will not cause any event.
  pressure->level = crosapi::mojom::MemoryPressureLevel::kNone;
  pressure->reclaim_target_kb = 0;
  evaluator->MemoryPressure(pressure->Clone());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator->current_vote());

  // Back into moderate.
  pressure->level = crosapi::mojom::MemoryPressureLevel::kModerate;
  pressure->reclaim_target_kb = 1000;
  evaluator->MemoryPressure(pressure->Clone());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

  // Now our events should be MODERATE, CRITICAL, MODERATE.
  ASSERT_EQ(2u, pressure_events.size());
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            pressure_events[0]);
  ASSERT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            pressure_events[1]);
  // Subsequent moderate notifications are throttled.
}

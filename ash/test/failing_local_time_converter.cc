// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/failing_local_time_converter.h"

#include "base/check.h"

namespace ash {

FailingLocalTimeConverter::FailingLocalTimeConverter() = default;

FailingLocalTimeConverter::~FailingLocalTimeConverter() = default;

bool FailingLocalTimeConverter::FromLocalExploded(
    const base::Time::Exploded& exploded,
    base::Time* time) const {
  CHECK(time);
  *time = base::Time();
  return false;
}

void FailingLocalTimeConverter::LocalExplode(
    base::Time time,
    base::Time::Exploded* exploded) const {
  CHECK(exploded);
  // Initialize all values here to prevent msan errors. This matches what
  // `base::Time::LocalExplode()` does when it fails.
  exploded->year = -1;
  exploded->month = -1;
  exploded->day_of_week = -1;
  exploded->day_of_month = -1;
  exploded->hour = -1;
  exploded->minute = -1;
  exploded->second = -1;
  exploded->millisecond = -1;
}

}  // namespace ash

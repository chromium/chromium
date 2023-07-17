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
  exploded->hour = -1;
}

}  // namespace ash

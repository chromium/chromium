// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_FAILING_LOCAL_TIME_CONVERTER_H_
#define ASH_TEST_FAILING_LOCAL_TIME_CONVERTER_H_

#include "ash/system/time/local_time_converter.h"
#include "base/time/time.h"

namespace ash {

// Simulates failure of all local time operations.
class FailingLocalTimeConverter : public LocalTimeConverter {
 public:
  FailingLocalTimeConverter();
  FailingLocalTimeConverter(const FailingLocalTimeConverter&) = delete;
  FailingLocalTimeConverter& operator=(const FailingLocalTimeConverter&) =
      delete;
  ~FailingLocalTimeConverter() override;

  // LocalTimeConverter implementation:
  //
  // Always returns false and sets `time` to null.
  bool FromLocalExploded(const base::Time::Exploded& exploded,
                         base::Time* time) const override;

  // Sets `exploded` to a series of invalid values.
  void LocalExplode(base::Time time,
                    base::Time::Exploded* exploded) const override;
};

}  // namespace ash

#endif  // ASH_TEST_FAILING_LOCAL_TIME_CONVERTER_H_

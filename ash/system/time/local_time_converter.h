// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_LOCAL_TIME_CONVERTER_H_
#define ASH_SYSTEM_TIME_LOCAL_TIME_CONVERTER_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

// Specifically created for b/285187343 so that tests can simulate scenarios
// where the caller has a valid `base::Time` value, but converting it to local
// time fails for inexplicable reasons (current guess is bad kernel state).
class ASH_EXPORT LocalTimeConverter {
 public:
  LocalTimeConverter();
  LocalTimeConverter(const LocalTimeConverter&) = delete;
  LocalTimeConverter& operator=(const LocalTimeConverter&) = delete;
  virtual ~LocalTimeConverter();

  // Calls the default implementation of all local time operations in
  // `base/time/time.h`. Always returns a non-null value.
  static const LocalTimeConverter& GetDefaultInstance();

  // The API below and its semantics directly mirror their equivalents in
  // `base/time/time.h`.
  virtual bool FromLocalExploded(const base::Time::Exploded& exploded,
                                 base::Time* time) const;
  virtual void LocalExplode(base::Time time,
                            base::Time::Exploded* exploded) const;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_LOCAL_TIME_CONVERTER_H_

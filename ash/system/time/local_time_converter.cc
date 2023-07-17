// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/local_time_converter.h"

#include "base/no_destructor.h"

namespace ash {

// static
const LocalTimeConverter& LocalTimeConverter::GetDefaultInstance() {
  static base::NoDestructor<LocalTimeConverter> g_local_time_converter;
  return *g_local_time_converter;
}

LocalTimeConverter::LocalTimeConverter() = default;

LocalTimeConverter::~LocalTimeConverter() = default;

bool LocalTimeConverter::FromLocalExploded(const base::Time::Exploded& exploded,
                                           base::Time* time) const {
  return base::Time::FromLocalExploded(exploded, time);
}

void LocalTimeConverter::LocalExplode(base::Time time,
                                      base::Time::Exploded* exploded) const {
  time.LocalExplode(exploded);
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_TEST_UTIL_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_TEST_UTIL_H_

#include "ash/ash_export.h"

namespace ash {

// Expects that scrims `exist` on all root windows with expected properties.
ASH_EXPORT void ExpectScrimsOnAllRootWindows(bool exist);

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_TEST_UTIL_H_

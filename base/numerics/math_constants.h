// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_MATH_CONSTANTS_H_
#define BASE_NUMERICS_MATH_CONSTANTS_H_

namespace base {

constexpr double kPiDouble = 3.14159265358979323846;
constexpr float kPiFloat = 3.14159265358979323846f;

// pi/180 and 180/pi. These are correctly rounded from the true
// mathematical value, unlike what you'd get from e.g.
// 180.0f / kPiFloat.
constexpr double kDegToRadDouble = 0.017453292519943295769;
constexpr float kDegToRadFloat = 0.017453292519943295769f;
constexpr double kRadToDegDouble = 57.295779513082320876798;
constexpr float kRadToDegFloat = 57.295779513082320876798f;

// sqrt(1/2) = 1/sqrt(2).
constexpr double kSqrtHalfDouble = 0.70710678118654752440;
constexpr float kSqrtHalfFloat = 0.70710678118654752440f;

// The mean acceleration due to gravity on Earth in m/s^2.
constexpr double kMeanGravityDouble = 9.80665;
constexpr float kMeanGravityFloat = 9.80665f;

}  // namespace base

#endif  // BASE_NUMERICS_MATH_CONSTANTS_H_

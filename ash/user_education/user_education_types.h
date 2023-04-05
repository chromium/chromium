// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_

namespace ash {

// Each value uniquely identifies an Ash feature tutorial.
enum class TutorialId {
  kMinValue,
  kCaptureModeTourPrototype1 = kMinValue,
  kCaptureModeTourPrototype2,
  kHoldingSpaceTourPrototype1,
  kHoldingSpaceTourPrototype2,
  kWelcomeTourPrototype1,
  kMaxValue = kWelcomeTourPrototype1,
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_FEATURE_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_FEATURE_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

// Base class for controllers responsible for specific user education features.
class ASH_EXPORT UserEducationFeatureController {
 public:
  UserEducationFeatureController(const UserEducationFeatureController&) =
      delete;
  UserEducationFeatureController& operator=(
      const UserEducationFeatureController&) = delete;
  virtual ~UserEducationFeatureController();

 protected:
  UserEducationFeatureController();
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_FEATURE_CONTROLLER_H_

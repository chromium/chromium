// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_FEATURE_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_FEATURE_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

enum class TutorialId;

// TODO(http://b/280840559): Remove tutorial concepts.
// Base class for controllers responsible for specific user education features.
// A single controller may be responsible for multiple feature tutorials.
class ASH_EXPORT UserEducationFeatureController {
 public:
  UserEducationFeatureController(const UserEducationFeatureController&) =
      delete;
  UserEducationFeatureController& operator=(
      const UserEducationFeatureController&) = delete;
  virtual ~UserEducationFeatureController();

  // Returns the descriptions for all feature tutorials controlled by this
  // instance, mapped to their respective identifiers.
  virtual std::map<TutorialId, user_education::TutorialDescription>
  GetTutorialDescriptions() = 0;

 protected:
  UserEducationFeatureController();
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_FEATURE_CONTROLLER_H_

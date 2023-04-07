// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_TUTORIAL_CONTROLLER_H_
#define ASH_USER_EDUCATION_TUTORIAL_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

enum class TutorialId;

// Base class for controllers responsible for specific feature tutorials. A
// single controller may be responsible for multiple feature tutorials.
class ASH_EXPORT TutorialController {
 public:
  TutorialController(const TutorialController&) = delete;
  TutorialController& operator=(const TutorialController&) = delete;
  virtual ~TutorialController();

  // Returns the descriptions for all feature tutorials controlled by this
  // instance, mapped to their respective identifiers.
  virtual std::map<TutorialId, user_education::TutorialDescription>
  GetTutorialDescriptions() = 0;

 protected:
  TutorialController();
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_TUTORIAL_CONTROLLER_H_

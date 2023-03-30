// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/user_education/tutorial_controller.h"

namespace ash {

// Controller responsible for Welcome Tour feature tutorials. Note that the
// `WelcomeTourController` is owned by the `UserEducationController` and exists
// if and only if the Welcome Tour feature is enabled.
class ASH_EXPORT WelcomeTourController : public TutorialController {
 public:
  WelcomeTourController();
  WelcomeTourController(const WelcomeTourController&) = delete;
  WelcomeTourController& operator=(const WelcomeTourController&) = delete;
  ~WelcomeTourController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Welcome Tour feature is enabled.
  static WelcomeTourController* Get();

 private:
  // TutorialController:
  std::map<user_education::TutorialIdentifier,
           user_education::TutorialDescription>
  GetTutorialDescriptions() override;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_

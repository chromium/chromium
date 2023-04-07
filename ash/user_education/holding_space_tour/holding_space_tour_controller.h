// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_HOLDING_SPACE_TOUR_HOLDING_SPACE_TOUR_CONTROLLER_H_
#define ASH_USER_EDUCATION_HOLDING_SPACE_TOUR_HOLDING_SPACE_TOUR_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/user_education/tutorial_controller.h"

namespace ash {

// Controller responsible for Holding Space Tour feature tutorials. Note that
// the `HoldingSpaceTourController` is owned by the `UserEducationController`
// and exists if and only if the Holding Space Tour feature is enabled.
class ASH_EXPORT HoldingSpaceTourController : public TutorialController {
 public:
  HoldingSpaceTourController();
  HoldingSpaceTourController(const HoldingSpaceTourController&) = delete;
  HoldingSpaceTourController& operator=(const HoldingSpaceTourController&) =
      delete;
  ~HoldingSpaceTourController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Holding Space Tour feature is enabled.
  static HoldingSpaceTourController* Get();

 private:
  // TutorialController:
  std::map<TutorialId, user_education::TutorialDescription>
  GetTutorialDescriptions() override;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_TOUR_HOLDING_SPACE_TOUR_CONTROLLER_H_

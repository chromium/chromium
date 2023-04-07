// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_CAPTURE_MODE_TOUR_CAPTURE_MODE_TOUR_CONTROLLER_H_
#define ASH_USER_EDUCATION_CAPTURE_MODE_TOUR_CAPTURE_MODE_TOUR_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/user_education/tutorial_controller.h"

namespace ash {

// Controller responsible for Capture Mode Tour feature tutorials. Note that the
// `CaptureModeTourController` is owned by the `UserEducationController` and
// exists if and only if the Capture Mode Tour feature is enabled.
class ASH_EXPORT CaptureModeTourController : public TutorialController {
 public:
  CaptureModeTourController();
  CaptureModeTourController(const CaptureModeTourController&) = delete;
  CaptureModeTourController& operator=(const CaptureModeTourController&) =
      delete;
  ~CaptureModeTourController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Capture Mode Tour feature is enabled.
  static CaptureModeTourController* Get();

 private:
  // TutorialController:
  std::map<TutorialId, user_education::TutorialDescription>
  GetTutorialDescriptions() override;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_CAPTURE_MODE_TOUR_CAPTURE_MODE_TOUR_CONTROLLER_H_

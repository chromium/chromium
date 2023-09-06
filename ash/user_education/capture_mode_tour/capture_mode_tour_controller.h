// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_CAPTURE_MODE_TOUR_CAPTURE_MODE_TOUR_CONTROLLER_H_
#define ASH_USER_EDUCATION_CAPTURE_MODE_TOUR_CAPTURE_MODE_TOUR_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/user_education/user_education_feature_controller.h"

namespace ash {

// Controller responsible for the Capture Mode Tour feature. Note that the
// `CaptureModeTourController` is owned by the `UserEducationController` and
// exists if and only if the Capture Mode Tour feature is enabled.
class ASH_EXPORT CaptureModeTourController
    : public UserEducationFeatureController {
 public:
  CaptureModeTourController();
  CaptureModeTourController(const CaptureModeTourController&) = delete;
  CaptureModeTourController& operator=(const CaptureModeTourController&) =
      delete;
  ~CaptureModeTourController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Capture Mode Tour feature is enabled.
  static CaptureModeTourController* Get();
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_CAPTURE_MODE_TOUR_CAPTURE_MODE_TOUR_CONTROLLER_H_

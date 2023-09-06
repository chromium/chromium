// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_feature_controller.h"

#include "ash/user_education/user_education_types.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {

UserEducationFeatureController::UserEducationFeatureController() = default;

UserEducationFeatureController::~UserEducationFeatureController() = default;

std::map<TutorialId, user_education::TutorialDescription>
UserEducationFeatureController::GetTutorialDescriptions() {
  return std::map<TutorialId, user_education::TutorialDescription>();
}

}  // namespace ash

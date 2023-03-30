// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_controller.h"

#include "base/check_op.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
WelcomeTourController* g_instance = nullptr;

}  // namespace

// WelcomeTourController -------------------------------------------------------

WelcomeTourController::WelcomeTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

WelcomeTourController::~WelcomeTourController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
WelcomeTourController* WelcomeTourController::Get() {
  return g_instance;
}

// TODO(http://b/275616974): Implement tutorial descriptions.
std::map<user_education::TutorialIdentifier,
         user_education::TutorialDescription>
WelcomeTourController::GetTutorialDescriptions() {
  std::map<user_education::TutorialIdentifier,
           user_education::TutorialDescription>
      tutorial_descriptions_by_id;
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple("AshWelcomeTourPrototype1"),
      std::forward_as_tuple());
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple("AshWelcomeTourPrototype2"),
      std::forward_as_tuple());
  return tutorial_descriptions_by_id;
}

}  // namespace ash

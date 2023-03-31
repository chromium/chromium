// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"

#include "base/check_op.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
HoldingSpaceTourController* g_instance = nullptr;

}  // namespace

// HoldingSpaceTourController --------------------------------------------------

HoldingSpaceTourController::HoldingSpaceTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

HoldingSpaceTourController::~HoldingSpaceTourController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
HoldingSpaceTourController* HoldingSpaceTourController::Get() {
  return g_instance;
}

// TODO(http://b/275909980): Implement tutorial descriptions.
std::map<user_education::TutorialIdentifier,
         user_education::TutorialDescription>
HoldingSpaceTourController::GetTutorialDescriptions() {
  std::map<user_education::TutorialIdentifier,
           user_education::TutorialDescription>
      tutorial_descriptions_by_id;
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple("AshHoldingSpaceTourPrototype1"),
      std::forward_as_tuple());
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple("AshHoldingSpaceTourPrototype2"),
      std::forward_as_tuple());
  return tutorial_descriptions_by_id;
}

}  // namespace ash

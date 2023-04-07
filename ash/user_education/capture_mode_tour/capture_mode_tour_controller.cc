// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/capture_mode_tour/capture_mode_tour_controller.h"

#include "ash/user_education/user_education_types.h"
#include "base/check_op.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
CaptureModeTourController* g_instance = nullptr;

}  // namespace

// CaptureModeTourController ---------------------------------------------------

CaptureModeTourController::CaptureModeTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

CaptureModeTourController::~CaptureModeTourController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CaptureModeTourController* CaptureModeTourController::Get() {
  return g_instance;
}

// TODO(http://b/275892879): Implement tutorial descriptions.
std::map<TutorialId, user_education::TutorialDescription>
CaptureModeTourController::GetTutorialDescriptions() {
  std::map<TutorialId, user_education::TutorialDescription>
      tutorial_descriptions_by_id;
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(TutorialId::kCaptureModeTourPrototype1),
      std::forward_as_tuple());
  tutorial_descriptions_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(TutorialId::kCaptureModeTourPrototype2),
      std::forward_as_tuple());
  return tutorial_descriptions_by_id;
}

}  // namespace ash

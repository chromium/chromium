// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/dictation_nudge_controller.h"

#include "ash/accessibility/dictation_nudge.h"

namespace ash {

DictationNudgeController::DictationNudgeController(
    const std::string& dictation_locale,
    const std::string& application_locale)
    : dictation_locale_(dictation_locale),
      application_locale_(application_locale) {}

DictationNudgeController::~DictationNudgeController() = default;

std::unique_ptr<SystemNudge> DictationNudgeController::CreateSystemNudge() {
  return std::make_unique<DictationNudge>(this);
}

}  // namespace ash
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/check.h"

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService() = default;
ContextualCueingService::~ContextualCueingService() = default;

void ContextualCueingService::RegisterCueTarget(
    CueTargetType type,
    std::unique_ptr<CueTarget> target) {
  cue_targets_.insert_or_assign(type, std::move(target));
}

void ContextualCueingService::OnClick(CueTargetType type, CueActionData data) {
  // TODO(crbug.com/498985205): record the click

  CueTarget* target = GetTarget(type);
  CHECK(target);
  target->OnClick(std::move(data));
}

void ContextualCueingService::OnDismiss(CueTargetType type) {
  // TODO(crbug.com/498985205): record the dismissal
}

CueTarget* ContextualCueingService::GetTarget(CueTargetType type) {
  auto iter = cue_targets_.find(type);
  return iter != cue_targets_.end() ? iter->second.get() : nullptr;
}

}  // namespace contextual_cueing

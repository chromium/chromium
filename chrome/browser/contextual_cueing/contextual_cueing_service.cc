// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService() = default;
ContextualCueingService::~ContextualCueingService() = default;

void ContextualCueingService::OnClick(CueTargetType type) {
  // TODO(crbug.com/498985205): record the click
}

void ContextualCueingService::OnDismiss(CueTargetType type) {
  // TODO(crbug.com/498985205): record the dismissal
}

}  // namespace contextual_cueing

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool.h"

#include "chrome/browser/actor/tools/observation_delay_type.h"

namespace actor {

ObservationDelayType Tool::GetObservationDelayType() const {
  return ObservationDelayType::kUseCompletionDelay;
}

}  // namespace actor

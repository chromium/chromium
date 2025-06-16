// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool.h"

#include <memory>

#include "chrome/browser/actor/tools/observation_delay_controller.h"

namespace actor {

std::unique_ptr<ObservationDelayController> Tool::GetObservationDelayer(
    content::RenderFrameHost& target_frame) const {
  return std::make_unique<ObservationDelayController>(target_frame);
}

bool Tool::RequiresFrame() const {
  return true;
}

}  // namespace actor

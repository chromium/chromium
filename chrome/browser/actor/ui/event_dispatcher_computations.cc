// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"

namespace actor::ui {
using base::UmaHistogramEnumeration;

AsyncUiEvent ComputedMouseMove(tabs::TabInterface::Handle tab,
                               const PageTarget& target) {
  if (std::holds_alternative<gfx::Point>(target)) {
    UmaHistogramEnumeration("Actor.EventDispatcher.ModelPageTargetType",
                            ModelPageTargetType::kPoint);
    return MouseMove(tab, std::get<gfx::Point>(target),
                     TargetSource::kToolRequest);
  }

  UmaHistogramEnumeration("Actor.EventDispatcher.ModelPageTargetType",
                          ModelPageTargetType::kDomNode);

  // TODO(crbug.com/434038099): Add conversion logic from DomNode to gfx::Point.

  // Create a move_event with no target point.
  return MouseMove(tab, std::nullopt, TargetSource::kUnresolvableInApc);
}

}  // namespace actor::ui

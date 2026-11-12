// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/conversions.h"

namespace glic {

GlicSidePanelCoordinator::ShowOptions ConvertToCoordinatorShowOptions(
    const ShowOptions& options,
    bool supports_peek) {
  GlicSidePanelCoordinator::ShowOptions target_options;

  const auto& side_panel_options =
      std::get<SidePanelShowOptions>(options.embedder_options);
  target_options.suppress_animations =
      side_panel_options.suppress_opening_animation;
  if (side_panel_options.prefer_peek && supports_peek) {
    target_options.initial_state =
        GlicSidePanelCoordinator::ShowOptions::InitialState::kPeeked;
  }
  target_options.open_trigger = side_panel_options.open_trigger;

  return target_options;
}

}  // namespace glic

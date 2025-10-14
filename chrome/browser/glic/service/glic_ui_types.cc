// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_ui_types.h"

#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_widget.h"

namespace glic {

ShowOptions::ShowOptions(EmbedderOptions embedder_options_in)
    : embedder_options(embedder_options_in) {}
ShowOptions::~ShowOptions() = default;

// static
ShowOptions ShowOptions::ForFloating(BrowserWindowInterface* anchor_browser) {
  return ForFloating(GlicWidget::GetInitialBounds(
      anchor_browser, GlicFloatingUi::GetDefaultSize()));
}

ShowOptions ShowOptions::ForFloating(gfx::Rect initial_bounds) {
  return ShowOptions{FloatingShowOptions{initial_bounds}};
}

ShowOptions ShowOptions::ForSidePanel(tabs::TabInterface& bound_tab) {
  return ShowOptions{SidePanelShowOptions{bound_tab}};
}

// end static

}  // namespace glic

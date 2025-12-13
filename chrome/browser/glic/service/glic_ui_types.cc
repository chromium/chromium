// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_ui_types.h"

#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_widget.h"

namespace glic {

std::string DescribeEmbedderKeyForTesting(const EmbedderKey& key) {
  return std::visit(absl::Overload(
                        [](const tabs::TabInterface* tab) {
                          return base::StringPrintf(
                              "Tab: %i", tab->GetHandle().raw_value());
                        },
                        [](const FloatingEmbedderKey& key) {
                          return std::string("Floating");
                        }),
                    key);
}

ShowOptions::ShowOptions(EmbedderOptions embedder_options_in)
    : embedder_options(embedder_options_in) {}
ShowOptions::ShowOptions(const ShowOptions&) = default;
ShowOptions::ShowOptions(ShowOptions&&) = default;
ShowOptions& ShowOptions::operator=(const ShowOptions&) = default;
ShowOptions::~ShowOptions() = default;

// static
ShowOptions ShowOptions::ForFloating(tabs::TabInterface::Handle source_tab,
                                     mojom::WebClientMode initial_mode) {
  BrowserWindowInterface* anchor_browser = nullptr;
  if (auto* tab = source_tab.Get()) {
    anchor_browser = tab->GetBrowserWindowInterface();
  }
  return ShowOptions{
      FloatingShowOptions{GlicWidget::GetInitialBounds(
                              anchor_browser, GlicFloatingUi::GetDefaultSize()),
                          source_tab, initial_mode}};
}

ShowOptions ShowOptions::ForFloating(gfx::Rect initial_bounds,
                                     mojom::WebClientMode initial_mode) {
  return ShowOptions{FloatingShowOptions{
      initial_bounds, tabs::TabInterface::Handle::Null(), initial_mode}};
}

ShowOptions ShowOptions::ForSidePanel(tabs::TabInterface& bound_tab) {
  return ShowOptions{SidePanelShowOptions{bound_tab}};
}

ShowOptions ShowOptions::ForSidePanel(tabs::TabInterface& bound_tab,
                                      GlicPinTrigger pin_trigger) {
  SidePanelShowOptions side_panel_options{bound_tab};
  side_panel_options.pin_trigger = pin_trigger;
  return ShowOptions{side_panel_options};
}

// end static

}  // namespace glic

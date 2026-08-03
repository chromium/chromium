// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_ui_types.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#endif

namespace glic {

std::string DescribeEmbedderKeyForTesting(const EmbedderKey& key) {
  return std::visit(
      absl::Overload(
          [](const TabEmbedderKey& key) { return std::string("Tab"); },
          [](const SidePanelEmbedderKey& key) {
            return base::StringPrintf("SidePanel: %i",
                                      key.tab->GetHandle().raw_value());
          },
          [](const FloatingEmbedderKey& key) {
            return std::string("Floating");
          }),
      key);
}

ShowOptions::ShowOptions(EmbedderOptions embedder_options_in)
    : embedder_options(std::move(embedder_options_in)) {}
ShowOptions::ShowOptions(EmbedderOptions embedder_options_in,
                         mojom::InvocationSource source_in)
    : invocation_source(source_in),
      embedder_options(std::move(embedder_options_in)) {}
ShowOptions::ShowOptions(const ShowOptions&) = default;
ShowOptions::ShowOptions(ShowOptions&&) = default;
ShowOptions& ShowOptions::operator=(const ShowOptions&) = default;
ShowOptions::~ShowOptions() = default;

// static
ShowOptions ShowOptions::ForFloating(tabs::TabInterface::Handle source_tab,
                                     mojom::WebClientMode initial_mode) {
// TODO:  Use an abstraction for the initial bounds and default size that can
// work for android and desktop. Also note that `GetBrowserWindowInterface`
// isn't yet available on the android `TabInterface`.
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* anchor_browser = nullptr;
  if (auto* tab = source_tab.Get()) {
    anchor_browser = tab->GetBrowserWindowInterface();
  }
  return ShowOptions{
      FloatingShowOptions{GlicWidget::GetInitialBounds(
                              anchor_browser, GlicFloatingUi::GetDefaultSize()),
                          source_tab, initial_mode}};
#else
  return ShowOptions{FloatingShowOptions{.source_tab = source_tab,
                                         .initial_mode = initial_mode}};
#endif
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

ShowOptions ShowOptions::ForSidePanel(
    tabs::TabInterface& bound_tab,
    GlicPinTrigger pin_trigger,
    mojom::InvocationSource invocation_source) {
  SidePanelShowOptions side_panel_options{bound_tab};
  side_panel_options.pin_trigger = pin_trigger;
  return ShowOptions{side_panel_options, invocation_source};
}

ShowOptions ShowOptions::ForTab(tabs::TabInterface& bound_tab) {
  return ShowOptions{TabShowOptions{bound_tab}};
}

// end static

SidePanelShowOptions::SidePanelShowOptions(tabs::TabInterface& bound_tab)
    : tab(bound_tab) {}
SidePanelShowOptions::SidePanelShowOptions(const SidePanelShowOptions&) =
    default;
SidePanelShowOptions::SidePanelShowOptions(SidePanelShowOptions&&) = default;
SidePanelShowOptions& SidePanelShowOptions::operator=(
    const SidePanelShowOptions&) = default;
SidePanelShowOptions::~SidePanelShowOptions() = default;

TabShowOptions::TabShowOptions() = default;
TabShowOptions::TabShowOptions(tabs::TabInterface& bound_tab)
    : tab_handle(bound_tab.GetHandle()) {}
TabShowOptions::TabShowOptions(tabs::TabHandle bound_tab_handle)
    : tab_handle(bound_tab_handle) {}
TabShowOptions::TabShowOptions(const TabShowOptions&) = default;
TabShowOptions::TabShowOptions(TabShowOptions&&) = default;
TabShowOptions& TabShowOptions::operator=(const TabShowOptions&) = default;
TabShowOptions::~TabShowOptions() = default;

}  // namespace glic

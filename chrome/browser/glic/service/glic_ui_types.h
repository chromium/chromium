// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_UI_TYPES_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_UI_TYPES_H_

#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/geometry/rect.h"

namespace glic {

// Key for the floating embedder. This is a struct to make it a distinct type
// for the variant, even though it's empty.
struct FloatingEmbedderKey {
  // Add a comparison operator to allow it to be a key in std::map/flat_map.
  auto operator<=>(const FloatingEmbedderKey&) const = default;
};

// A key representing a unique embedder.
using EmbedderKey = std::variant<tabs::TabInterface*, FloatingEmbedderKey>;
std::string DescribeEmbedderKeyForTesting(const EmbedderKey& key);

struct SidePanelShowOptions {
  explicit SidePanelShowOptions(tabs::TabInterface& bound_tab)
      : tab(bound_tab) {}
  base::raw_ref<tabs::TabInterface> tab;
  bool suppress_opening_animation = false;
};

struct FloatingShowOptions {
  gfx::Rect initial_bounds;
  tabs::TabInterface::Handle source_tab;
  mojom::WebClientMode initial_mode = mojom::WebClientMode::kUnknown;
};

using EmbedderOptions = std::variant<SidePanelShowOptions, FloatingShowOptions>;
struct ShowOptions {
  explicit ShowOptions(EmbedderOptions panel_options);
  explicit ShowOptions(EmbedderOptions panel_options, bool focus);
  ShowOptions(const ShowOptions&);
  ShowOptions(ShowOptions&&);
  ShowOptions& operator=(const ShowOptions&);
  ~ShowOptions();

  // Uses `anchor_browser` to get initial location. If `anchor_browser` is
  // nullptr, it will use default location values.
  static ShowOptions ForFloating(
      tabs::TabInterface::Handle source_tab,
      mojom::WebClientMode initial_mode = mojom::WebClientMode::kUnknown);
  static ShowOptions ForFloating(
      gfx::Rect initial_bounds,
      mojom::WebClientMode initial_mode = mojom::WebClientMode::kUnknown);
  static ShowOptions ForSidePanel(tabs::TabInterface& bound_tab);

  // Shared show options
  bool focus_on_show = false;
  bool reinitialize_if_already_active = false;

  // Container for options that are different between side panel and floaty.
  EmbedderOptions embedder_options;
};

inline EmbedderKey GetEmbedderKey(const ShowOptions& options) {
  return std::visit(absl::Overload{[](const SidePanelShowOptions& opts) {
                                     return EmbedderKey(&opts.tab.get());
                                   },
                                   [](const FloatingShowOptions& opts) {
                                     return EmbedderKey(FloatingEmbedderKey());
                                   }},
                    options.embedder_options);
}

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_UI_TYPES_H_

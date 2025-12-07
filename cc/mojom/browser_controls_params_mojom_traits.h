// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_PARAMS_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_PARAMS_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "cc/mojom/browser_controls_params.mojom-shared.h"
#include "cc/trees/browser_controls_params.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(CC_SHARED_MOJOM_TRAITS)
    StructTraits<cc::mojom::BrowserControlsParamsDataView,
                 cc::BrowserControlsParams> {
  static float top_controls_height(const cc::BrowserControlsParams& params) {
    return params.top_controls_height;
  }
  static float top_controls_min_height(
      const cc::BrowserControlsParams& params) {
    return params.top_controls_min_height;
  }
  static float bottom_controls_height(const cc::BrowserControlsParams& params) {
    return params.bottom_controls_height;
  }
  static float bottom_controls_min_height(
      const cc::BrowserControlsParams& params) {
    return params.bottom_controls_min_height;
  }
  static bool animate_browser_controls_height_changes(
      const cc::BrowserControlsParams& params) {
    return params.animate_browser_controls_height_changes;
  }
  static bool browser_controls_shrink_blink_size(
      const cc::BrowserControlsParams& params) {
    return params.browser_controls_shrink_blink_size;
  }
  static bool only_expand_top_controls_at_page_top(
      const cc::BrowserControlsParams& params) {
    return params.only_expand_top_controls_at_page_top;
  }

  static bool Read(cc::mojom::BrowserControlsParamsDataView data,
                   cc::BrowserControlsParams* out) {
    out->top_controls_height = data.top_controls_height();
    out->top_controls_min_height = data.top_controls_min_height();
    out->bottom_controls_height = data.bottom_controls_height();
    out->bottom_controls_min_height = data.bottom_controls_min_height();
    out->animate_browser_controls_height_changes =
        data.animate_browser_controls_height_changes();
    out->browser_controls_shrink_blink_size =
        data.browser_controls_shrink_blink_size();
    out->only_expand_top_controls_at_page_top =
        data.only_expand_top_controls_at_page_top();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_PARAMS_MOJOM_TRAITS_H_

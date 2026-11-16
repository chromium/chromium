// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_SNAP_HINT_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_SNAP_HINT_MOJOM_TRAITS_H_

#include "base/check.h"
#include "base/notreached.h"
#include "cc/input/browser_controls_snap_hint.h"
#include "cc/mojom/browser_controls_snap_hint.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::BrowserControlsSnapHintAction,
                  cc::BrowserControlsSnapHint::Action> {
  static cc::mojom::BrowserControlsSnapHintAction ToMojom(
      cc::BrowserControlsSnapHint::Action input) {
    switch (input) {
      case cc::BrowserControlsSnapHint::Action::kMustShow:
        return cc::mojom::BrowserControlsSnapHintAction::kMustShow;
      case cc::BrowserControlsSnapHint::Action::kCanShow:
        return cc::mojom::BrowserControlsSnapHintAction::kCanShow;
      case cc::BrowserControlsSnapHint::Action::kCanHide:
        return cc::mojom::BrowserControlsSnapHintAction::kCanHide;
    }
    NOTREACHED();
  }

  static cc::BrowserControlsSnapHint::Action FromMojom(
      cc::mojom::BrowserControlsSnapHintAction input) {
    switch (input) {
      case cc::mojom::BrowserControlsSnapHintAction::kMustShow:
        return cc::BrowserControlsSnapHint::Action::kMustShow;
      case cc::mojom::BrowserControlsSnapHintAction::kCanShow:
        return cc::BrowserControlsSnapHint::Action::kCanShow;
      case cc::mojom::BrowserControlsSnapHintAction::kCanHide:
        return cc::BrowserControlsSnapHint::Action::kCanHide;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<cc::mojom::BrowserControlsSnapHintDataView,
                    cc::BrowserControlsSnapHint> {
  static cc::BrowserControlsSnapHint::Action action(
      const cc::BrowserControlsSnapHint& hint) {
    return hint.action;
  }
  static float slowness(const cc::BrowserControlsSnapHint& hint) {
    // Read() rejects a slowness outside [0, 1], and the browser process then
    // terminates the renderer for a bad message. CHECK the same condition so
    // that the renderer crashes where the invalid hint is sent.
    CHECK(hint.slowness >= 0.0f && hint.slowness <= 1.0f);
    return hint.slowness;
  }
  static bool is_scroll_end_or_inertial(
      const cc::BrowserControlsSnapHint& hint) {
    return hint.is_scroll_end_or_inertial;
  }

  static bool Read(cc::mojom::BrowserControlsSnapHintDataView data,
                   cc::BrowserControlsSnapHint* out) {
    if (!(data.slowness() >= 0.0f && data.slowness() <= 1.0f)) {
      return false;
    }
    if (!data.ReadAction(&out->action)) {
      return false;
    }
    out->slowness = data.slowness();
    out->is_scroll_end_or_inertial = data.is_scroll_end_or_inertial();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_SNAP_HINT_MOJOM_TRAITS_H_

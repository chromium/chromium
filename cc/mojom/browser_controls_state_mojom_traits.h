// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_STATE_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_STATE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "cc/input/browser_controls_state.h"
#include "cc/mojom/browser_controls_state.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::BrowserControlsState, cc::BrowserControlsState> {
  static cc::mojom::BrowserControlsState ToMojom(
      cc::BrowserControlsState input) {
    switch (input) {
      case cc::BrowserControlsState::kShown:
        return cc::mojom::BrowserControlsState::kShown;
      case cc::BrowserControlsState::kHidden:
        return cc::mojom::BrowserControlsState::kHidden;
      case cc::BrowserControlsState::kBoth:
        return cc::mojom::BrowserControlsState::kBoth;
    }
    NOTREACHED();
  }

  static cc::BrowserControlsState FromMojom(
      cc::mojom::BrowserControlsState input) {
    switch (input) {
      case cc::mojom::BrowserControlsState::kShown:
        return cc::BrowserControlsState::kShown;
      case cc::mojom::BrowserControlsState::kHidden:
        return cc::BrowserControlsState::kHidden;
      case cc::mojom::BrowserControlsState::kBoth:
        return cc::BrowserControlsState::kBoth;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_STATE_MOJOM_TRAITS_H_

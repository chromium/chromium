// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_TOUCH_ACTION_MOJOM_TRAITS_H_
#define CC_MOJOM_TOUCH_ACTION_MOJOM_TRAITS_H_

#include <stdint.h>

#include "cc/input/touch_action.h"
#include "cc/mojom/touch_action.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::TouchActionDataView, cc::TouchAction> {
  static uint32_t value(const cc::TouchAction& input) {
    return static_cast<uint32_t>(input);
  }

  static bool Read(cc::mojom::TouchActionDataView data, cc::TouchAction* out) {
    uint32_t value = data.value();
    if (value > static_cast<uint32_t>(cc::TouchAction::kMax)) {
      return false;
    }
    *out = static_cast<cc::TouchAction>(value);
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_TOUCH_ACTION_MOJOM_TRAITS_H_

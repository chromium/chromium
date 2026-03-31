// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_HIT_TEST_OPAQUENESS_MOJOM_TRAITS_H_
#define CC_MOJOM_HIT_TEST_OPAQUENESS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "cc/input/hit_test_opaqueness.h"
#include "cc/mojom/hit_test_opaqueness.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::HitTestOpaqueness, cc::HitTestOpaqueness> {
  static cc::mojom::HitTestOpaqueness ToMojom(cc::HitTestOpaqueness input) {
    switch (input) {
      case cc::HitTestOpaqueness::kTransparent:
        return cc::mojom::HitTestOpaqueness::kTransparent;
      case cc::HitTestOpaqueness::kMixed:
        return cc::mojom::HitTestOpaqueness::kMixed;
      case cc::HitTestOpaqueness::kOpaque:
        return cc::mojom::HitTestOpaqueness::kOpaque;
      default:
        NOTREACHED();
    }
  }

  static cc::HitTestOpaqueness FromMojom(cc::mojom::HitTestOpaqueness input) {
    switch (input) {
      case cc::mojom::HitTestOpaqueness::kTransparent:
        return cc::HitTestOpaqueness::kTransparent;
      case cc::mojom::HitTestOpaqueness::kMixed:
        return cc::HitTestOpaqueness::kMixed;
      case cc::mojom::HitTestOpaqueness::kOpaque:
        return cc::HitTestOpaqueness::kOpaque;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_HIT_TEST_OPAQUENESS_MOJOM_TRAITS_H_

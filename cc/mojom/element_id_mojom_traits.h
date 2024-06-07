// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_ELEMENT_ID_MOJOM_TRAITS_H_
#define CC_MOJOM_ELEMENT_ID_MOJOM_TRAITS_H_

#include "cc/mojom/element_id.mojom-shared.h"
#include "cc/paint/element_id.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::ElementIdDataView, cc::ElementId> {
  static uint64_t id(const cc::ElementId& id) { return id.GetInternalValue(); }

  static bool IsNull(const cc::ElementId& id) { return !id; }

  static void SetToNull(cc::ElementId* out) { *out = cc::ElementId(); }

  static bool Read(cc::mojom::ElementIdDataView data, cc::ElementId* out) {
    if (data.id() == cc::ElementId::kInvalidElementId) {
      *out = cc::ElementId();
    } else {
      *out = cc::ElementId(data.id());
    }
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_ELEMENT_ID_MOJOM_TRAITS_H_

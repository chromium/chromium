// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_UI_RESOURCE_ID_MOJOM_TRAITS_H_
#define CC_MOJOM_UI_RESOURCE_ID_MOJOM_TRAITS_H_

#include "cc/mojom/ui_resource_id.mojom-shared.h"
#include "cc/resources/ui_resource_client.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::UIResourceIdDataView, cc::UIResourceId> {
  static int32_t value(const cc::UIResourceId& id) {
    // We cannot send resource ids that are uninitialized.
    DCHECK_NE(id, cc::UIResourceClient::kUninitializedUIResourceId);
    return static_cast<int32_t>(id);
  }

  static bool Read(cc::mojom::UIResourceIdDataView data,
                   cc::UIResourceId* out) {
    cc::UIResourceId result(data.value());
    // We cannot receive resource ids that are uninitialized.
    if (result == cc::UIResourceClient::kUninitializedUIResourceId) {
      return false;
    }
    *out = result;
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_UI_RESOURCE_ID_MOJOM_TRAITS_H_

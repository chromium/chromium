// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_SYNCED_SCROLL_OFFSET_MOJOM_TRAITS_H_
#define CC_MOJOM_SYNCED_SCROLL_OFFSET_MOJOM_TRAITS_H_

#include "base/memory/scoped_refptr.h"
#include "cc/mojom/synced_scroll_offset.mojom-shared.h"
#include "cc/trees/property_tree.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SyncedScrollOffsetDataView,
                    scoped_refptr<cc::SyncedScrollOffset>> {
  static bool IsNull(const scoped_refptr<cc::SyncedScrollOffset>& r) {
    return !r;
  }

  static void SetToNull(scoped_refptr<cc::SyncedScrollOffset>* out) {
    out->reset();
  }

  static gfx::PointF scroll_offset(
      const scoped_refptr<cc::SyncedScrollOffset>& r) {
    return r->Current(/*is_active_tree=*/true);
  }

  static bool Read(cc::mojom::SyncedScrollOffsetDataView data,
                   scoped_refptr<cc::SyncedScrollOffset>* out);
};

}  // namespace mojo

#endif  // CC_MOJOM_SYNCED_SCROLL_OFFSET_MOJOM_TRAITS_H_

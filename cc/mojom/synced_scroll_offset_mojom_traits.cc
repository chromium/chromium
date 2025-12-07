// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/synced_scroll_offset_mojom_traits.h"

#include <utility>

#include "cc/mojom/synced_scroll_offset.mojom.h"

namespace mojo {

bool StructTraits<cc::mojom::SyncedScrollOffsetDataView,
                  scoped_refptr<cc::SyncedScrollOffset>>::
    Read(cc::mojom::SyncedScrollOffsetDataView data,
         scoped_refptr<cc::SyncedScrollOffset>* out) {
  gfx::PointF scroll_offset;
  if (!data.ReadScrollOffset(&scroll_offset)) {
    return false;
  }
  *out = base::MakeRefCounted<cc::SyncedScrollOffset>();
  (*out)->SetCurrent(scroll_offset);
  return true;
}

}  // namespace mojo

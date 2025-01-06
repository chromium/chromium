// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_gdi_object.h"

#include <windows.h>

namespace base::win::internal {

#define DEFINE_TRAIT_SPECIALIZATION(T)            \
  template <>                                     \
  void ScopedGDIObjectTraits<T>::Free(T object) { \
    ::DeleteObject(object);                       \
  }
DEFINE_TRAIT_SPECIALIZATION(HBITMAP)
DEFINE_TRAIT_SPECIALIZATION(HBRUSH)
DEFINE_TRAIT_SPECIALIZATION(HFONT)
DEFINE_TRAIT_SPECIALIZATION(HPEN)
DEFINE_TRAIT_SPECIALIZATION(HRGN)
#undef DEFINE_TRAIT_SPECIALIZATION

// `HICON` must be freed via `::DestroyIcon()` instead.
template <>
void ScopedGDIObjectTraits<HICON>::Free(HICON object) {
  ::DestroyIcon(object);
}

}  // namespace base::win::internal

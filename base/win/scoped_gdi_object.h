// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_GDI_OBJECT_H_
#define BASE_WIN_SCOPED_GDI_OBJECT_H_

#include <windows.h>

#include "base/scoped_generic.h"

namespace base {
namespace win {

namespace internal {

template <class T>
struct ScopedGDIObjectTraits {
  static T InvalidValue() { return nullptr; }
  static void Free(T object) { DeleteObject(object); }
};

// An explicit specialization for HICON because we have to call DestroyIcon()
// instead of DeleteObject() for HICON.
template <>
void inline ScopedGDIObjectTraits<HICON>::Free(HICON icon) {
  DestroyIcon(icon);
}

}  // namespace internal

// Like ScopedHandle but for GDI objects.
template <class T>
using ScopedGDIObject = ScopedGeneric<T, internal::ScopedGDIObjectTraits<T>>;

// Typedefs for some common use cases.
typedef ScopedGDIObject<HBITMAP> ScopedBitmap;
typedef ScopedGDIObject<HRGN> ScopedRegion;
typedef ScopedGDIObject<HFONT> ScopedHFONT;
typedef ScopedGDIObject<HICON> ScopedHICON;

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_GDI_OBJECT_H_

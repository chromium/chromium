// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_GDI_OBJECT_H_
#define BASE_WIN_SCOPED_GDI_OBJECT_H_

#include "base/base_export.h"
#include "base/scoped_generic.h"
#include "base/types/always_false.h"
#include "base/win/win_handle_types.h"

namespace base::win {

namespace internal {

template <typename T>
struct BASE_EXPORT ScopedGDIObjectTraits {
  static T InvalidValue() { return nullptr; }
  static void Free(T object) {
    static_assert(base::AlwaysFalse<T>, "Explicitly forward-declare this T");
  }
};

// Forward-declare all used specializations and define them in the .cc file.
// This avoids pulling `<windows.h>` transiently into every file that
// `#include`s this one.
#define DECLARE_TRAIT_SPECIALIZATION(T) \
  template <>                           \
  void ScopedGDIObjectTraits<T>::Free(T object);
DECLARE_TRAIT_SPECIALIZATION(HBITMAP)
DECLARE_TRAIT_SPECIALIZATION(HBRUSH)
DECLARE_TRAIT_SPECIALIZATION(HFONT)
DECLARE_TRAIT_SPECIALIZATION(HICON)
DECLARE_TRAIT_SPECIALIZATION(HPEN)
DECLARE_TRAIT_SPECIALIZATION(HRGN)
#undef DECLARE_TRAIT_SPECIALIZATION

}  // namespace internal

// Like ScopedHandle but for GDI objects.
template <class T>
using ScopedGDIObject = ScopedGeneric<T, internal::ScopedGDIObjectTraits<T>>;

// Typedefs for some common use cases.
typedef ScopedGDIObject<HBITMAP> ScopedBitmap;
typedef ScopedGDIObject<HRGN> ScopedRegion;
typedef ScopedGDIObject<HFONT> ScopedHFONT;
typedef ScopedGDIObject<HICON> ScopedHICON;

}  // namespace base::win

#endif  // BASE_WIN_SCOPED_GDI_OBJECT_H_

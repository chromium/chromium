// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIT_CAST_H_
#define BASE_BIT_CAST_H_

#include "base/compiler_specific.h"

#if !HAS_BUILTIN(__builtin_bit_cast)
#include <string.h>
#include "base/template_util.h"
#endif

// This is C++20's std::bit_cast<>().
// It morally does what `*reinterpret_cast<Dest*>(&source)` does, but the cast/deref pair
// is undefined behavior, while bit_cast<>() isn't.
template <class Dest, class Source>
#if HAS_BUILTIN(__builtin_bit_cast)
constexpr
#else
inline
#endif
    Dest
    bit_cast(const Source& source) {
#if HAS_BUILTIN(__builtin_bit_cast)
  // TODO(thakis): Keep only this codepath once nacl is gone or updated.
  return __builtin_bit_cast(Dest, source);
#else
  static_assert(sizeof(Dest) == sizeof(Source),
                "bit_cast requires source and destination to be the same size");
  static_assert(base::is_trivially_copyable<Dest>::value,
                "bit_cast requires the destination type to be copyable");
  static_assert(base::is_trivially_copyable<Source>::value,
                "bit_cast requires the source type to be copyable");

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
#endif
}

#endif  // BASE_BIT_CAST_H_

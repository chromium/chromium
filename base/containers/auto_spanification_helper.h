// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_AUTO_SPANIFICATION_HELPER_H_
#define BASE_CONTAINERS_AUTO_SPANIFICATION_HELPER_H_

#include "base/numerics/checked_math.h"

namespace base::spanification_internal {

// ToPointer is a helper function that converts either of a `T&` or a `T*` to a
// pointer `T*`.
//
// Example) Given the following two cases of spanification rewriting,
//     obj.method(arg...)  ==> MACRO(obj, arg...)
//     ptr->method(arg...) ==> MACRO(ptr, arg...)
// MACRO takes either of an optionally-const T& or T* value as the receiver
// object argument. ToPointer(obj) / ToPointer(ptr) converts them to a pointer
// type value. This helps avoiding implementing two versions of the macro.
//
// Note: This helper is intended to be used only in the following macros. Do not
// use this helper outside of them.

template <typename T>
inline const T* ToPointer(const T* value) {
  return value;
}

template <typename T>
inline T* ToPointer(T* value) {
  return value;
}

template <typename T>
inline const T* ToPointer(const T& value) {
  return &value;
}

template <typename T>
inline T* ToPointer(T& value) {
  return &value;
}

// If the value is a smart pointer type value, returns just the value.

template <typename T>
  requires requires(T t) { t.operator->(); }
inline const T& ToPointer(const T& value) {
  return value;
}

template <typename T>
  requires requires(T t) { t.operator->(); }
inline T& ToPointer(T& value) {
  return value;
}

}  // namespace base::spanification_internal

// The following helper macros are introduced temporarily in order to help the
// auto spanification tool (//tools/clang/spanify). The macros wrap third-party
// API calls which should return a base::span for safety but actually not. In
// the future, these macro calls should be replaced with new spanified APIs.
//
// The helper macros are macros because this header (in base/) cannot depend on
// non-base (especially third-party) libraries. The call sites must include
// necessary headers on their side.
//
// In the following macro definitions, a temporary lambda expression is used in
// order to not evaluate arguments multiple times. It also introduces a C++ code
// block where we can define temporary variables.

// https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/include/core/SkBitmap.h;drc=f72bd467feb15edd9323e46eab1b74ab6025bc5b;l=936
#define UNSAFE_SKBITMAP_GETADDR32(arg_self, arg_x, arg_y) \
  ([](auto&& self, int x, int y) {                        \
    uint32_t* row = self->getAddr32(x, y);                \
    ::base::CheckedNumeric<size_t> width = self->width(); \
    size_t size = (width - x).ValueOrDie();               \
    return UNSAFE_TODO(base::span<uint32_t>(row, size));  \
  }(::base::spanification_internal::ToPointer(arg_self), arg_x, arg_y))

// https://source.chromium.org/chromium/chromium/src/+/main:third_party/perl/c/include/harfbuzz/hb-buffer.h;drc=6f3e5028eb65d0b4c5fdd792106ac4c84eee1eb3;l=442
#define UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(arg_buffer, arg_length)        \
  ([](hb_buffer_t* buffer, unsigned int* length) {                          \
    unsigned int len;                                                       \
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, &len); \
    if (length)                                                             \
      *length = len;                                                        \
    /* It's not clear whether the length is guaranteed to be 0 when !pos.   \
       Explicitly set the length to 0 just in case. */                      \
    if (!pos)                                                               \
      return UNSAFE_TODO(base::span<hb_glyph_position_t>(pos, 0u));         \
    return UNSAFE_TODO(base::span<hb_glyph_position_t>(pos, len));          \
  }(arg_buffer, arg_length))

#endif  // BASE_CONTAINERS_AUTO_SPANIFICATION_HELPER_H_

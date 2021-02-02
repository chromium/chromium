// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_CHAR_TRAITS_H_
#define BASE_STRINGS_CHAR_TRAITS_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"

namespace base {

// constexpr version of http://en.cppreference.com/w/cpp/string/char_traits.
// This currently just implements the bits needed to support a (mostly)
// constexpr StringPiece.
//
// TODO(dcheng): Once we switch to C++17, most methods will become constexpr and
// we can switch over to using the one in the standard library.
template <typename T>
struct CharTraits {
  // Performs a lexographical comparison of the first N characters of |s1| and
  // |s2|. Returns 0 if equal, -1 if |s1| is less than |s2|, and 1 if |s1| is
  // greater than |s2|.
  static constexpr int compare(const T* s1, const T* s2, size_t n) noexcept;

  // Returns the length of |s|, assuming null termination (and not including the
  // terminating null).
  static constexpr size_t length(const T* s) noexcept;

  // Searches for character |c| within the first |n| characters of the sequence
  // pointed to by |s|.
  static constexpr const T* find(const T* s, size_t n, T c);
};

template <typename T>
constexpr int CharTraits<T>::compare(const T* s1,
                                     const T* s2,
                                     size_t n) noexcept {
  // Comparison with operator < fails, because of signed/unsigned
  // mismatch, https://crbug.com/941696
  // std::char_traits<T>::lt is guaranteed to be constexpr in C++14:
  // https://timsong-cpp.github.io/cppwp/n4140/char.traits.specializations#char
  for (; n; --n, ++s1, ++s2) {
    if (std::char_traits<T>::lt(*s1, *s2))
      return -1;
    if (std::char_traits<T>::lt(*s2, *s1))
      return 1;
  }
  return 0;
}

template <typename T>
constexpr size_t CharTraits<T>::length(const T* s) noexcept {
  size_t i = 0;
  for (; *s; ++s)
    ++i;
  return i;
}

template <typename T>
constexpr const T* CharTraits<T>::find(const T* s, size_t n, T c) {
  for (; n; --n, ++s) {
    if (std::char_traits<T>::eq(*s, c))
      return s;
  }
  return nullptr;
}

// char and wchar_t specialization of CharTraits that can use clang's constexpr
// instrinsics, where available.
#if HAS_FEATURE(cxx_constexpr_string_builtins)
template <>
struct CharTraits<char> {
  static constexpr int compare(const char* s1,
                               const char* s2,
                               size_t n) noexcept {
    return __builtin_memcmp(s1, s2, n);
  }

  static constexpr size_t length(const char* s) noexcept {
    return __builtin_strlen(s);
  }

  static constexpr const char* find(const char* s, size_t n, char c) {
    return __builtin_char_memchr(s, c, n);
  }
};

template <>
struct CharTraits<wchar_t> {
  static constexpr int compare(const wchar_t* s1,
                               const wchar_t* s2,
                               size_t n) noexcept {
    return __builtin_wmemcmp(s1, s2, n);
  }

  static constexpr size_t length(const wchar_t* s) noexcept {
    return __builtin_wcslen(s);
  }

  static constexpr const wchar_t* find(const wchar_t* s, size_t n, wchar_t c) {
    return __builtin_wmemchr(s, c, n);
  }
};
#endif

}  // namespace base

#endif  // BASE_STRINGS_CHAR_TRAITS_H_

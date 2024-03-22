// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_IS_BASIC_CSTRING_VIEW_H_
#define BASE_STRINGS_IS_BASIC_CSTRING_VIEW_H_

#include <cstddef>

namespace base {
template <class Char>
class basic_cstring_view;
}

namespace base::internal {

// A helper struct for writing templates against all basic_cstring_view
// types at once, without having to know/spell them all (such as cstring_view,
// wcstring_view, etc).
template <class T>
struct IsBasicCStringView {
  static constexpr bool value = false;
};

template <class T>
  requires(std::same_as<T, basic_cstring_view<typename T::value_type>>)
struct IsBasicCStringView<T> {
  static constexpr bool value = true;
};

}  // namespace base::internal

#endif

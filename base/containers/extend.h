// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_EXTEND_H_
#define BASE_CONTAINERS_EXTEND_H_

#include <iterator>
#include <type_traits>
#include <vector>

#include "base/containers/span.h"

namespace base {

// Append to |dst| all elements of |src| by std::move-ing them out of |src|.
// After this operation, |src| will be empty.
template <typename T>
void Extend(std::vector<T>& dst, std::vector<T>&& src) {
  dst.insert(dst.end(), std::make_move_iterator(src.begin()),
             std::make_move_iterator(src.end()));
  src.clear();
}

// Append to |dst| all elements of |src| by copying them out of |src|. |src| is
// not changed.
//
// # Implementation note on std::type_identity_t:
// This overload allows implicit conversions to `span<const T>`, by creating a
// non-deduced context:
// https://en.cppreference.com/w/cpp/language/template_argument_deduction#Non-deduced_contexts
//
// This would not be possible by just receiving `span<const T>` as the templated
// `T` can not be deduced (even though it is fixed by the deduction from the
// `vector<T>` parameter).
template <typename T>
void Extend(std::vector<T>& dst, std::type_identity_t<span<const T>> src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

}  // namespace base

#endif  // BASE_CONTAINERS_EXTEND_H_

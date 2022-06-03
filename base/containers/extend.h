// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_EXTEND_H_
#define BASE_CONTAINERS_EXTEND_H_

#include <iterator>
#include <vector>

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
template <typename T>
void Extend(std::vector<T>& dst, const std::vector<T>& src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

}  // namespace base

#endif  // BASE_CONTAINERS_EXTEND_H_

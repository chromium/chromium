// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/not_fn.h"

#include <functional>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(FunctionalTest, NotFn) {
  constexpr auto not_less = base::not_fn(std::less<>());

  using NotLessT = decltype(not_less);
  static_assert(static_cast<const NotLessT&>(not_less)(1, 1), "");
  static_assert(static_cast<NotLessT&>(not_less)(2, 1), "");
  static_assert(static_cast<const NotLessT&&>(not_less)(2, 2), "");
  static_assert(!static_cast<NotLessT&&>(not_less)(1, 2), "");
}

}  // namespace base

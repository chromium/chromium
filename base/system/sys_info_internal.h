// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_SYS_INFO_INTERNAL_H_
#define BASE_SYSTEM_SYS_INFO_INTERNAL_H_

#include "base/macros.h"

namespace base {

namespace internal {

template <typename T, T (*F)(void)>
class LazySysInfoValue {
 public:
  LazySysInfoValue() : value_(F()) {}

  LazySysInfoValue(const LazySysInfoValue&) = delete;
  LazySysInfoValue& operator=(const LazySysInfoValue&) = delete;

  ~LazySysInfoValue() = default;

  T value() { return value_; }

 private:
  const T value_;
};

}  // namespace internal

}  // namespace base

#endif  // BASE_SYSTEM_SYS_INFO_INTERNAL_H_

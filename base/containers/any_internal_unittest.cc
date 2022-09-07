// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/any_internal.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {
struct OutOfLineStruct {
  void* one;
  void* two;
  void* three;
  void* four;
};
}  // namespace

TEST(AnyInternalTest, InlineOrOutlineStorage) {
  static_assert(AnyInternal::InlineStorageHelper<int>::kUseInlineStorage,
                "int should be stored inline");
  static_assert(AnyInternal::InlineStorageHelper<int*>::kUseInlineStorage,
                "int* should be stored inline");
  static_assert(
      AnyInternal::InlineStorageHelper<std::unique_ptr<int>>::kUseInlineStorage,
      "std::unique_ptr<int> should be stored inline");
  static_assert(
      !AnyInternal::InlineStorageHelper<OutOfLineStruct>::kUseInlineStorage,
      "A struct with four pointers should be stored out of line");
}

}  // namespace internal
}  // namespace base

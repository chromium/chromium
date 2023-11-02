// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/reference.h"

#include <windows.foundation.h>
#include <wrl/client.h>

#include "testing/gtest/include/gtest/gtest.h"

#ifdef NTDDI_WIN10_VB  // Windows 10.0.19041
// Specialization templates that used to be in windows.foundation.h, removed in
// the 10.0.19041.0 SDK, so placed here instead.
namespace ABI {
namespace Windows {
namespace Foundation {
template <>
struct __declspec(uuid("3c00fd60-2950-5939-a21a-2d12c5a01b8a")) IReference<bool>
    : IReference_impl<Internal::AggregateType<bool, boolean>> {};

template <>
struct __declspec(uuid("548cefbd-bc8a-5fa0-8df2-957440fc8bf4")) IReference<int>
    : IReference_impl<int> {};
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI
#endif

namespace base {
namespace win {

namespace {

using Microsoft::WRL::Make;

}  // namespace

TEST(ReferenceTest, Value) {
  auto ref = Make<Reference<int>>(123);
  int value = 0;
  HRESULT hr = ref->get_Value(&value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(123, value);
}

TEST(ReferenceTest, ValueAggregate) {
  auto ref = Make<Reference<bool>>(true);
  boolean value = false;
  HRESULT hr = ref->get_Value(&value);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(value);
}

}  // namespace win
}  // namespace base

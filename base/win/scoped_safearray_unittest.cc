// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_safearray.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(ScopedSafearrayTest, ScopedSafearrayMethods) {
  ScopedSafearray empty_safe_array;
  EXPECT_EQ(empty_safe_array.Get(), nullptr);
  EXPECT_EQ(empty_safe_array.Release(), nullptr);
  EXPECT_NE(empty_safe_array.Receive(), nullptr);

  SAFEARRAY* safe_array = SafeArrayCreateVector(
      VT_R8 /* element type */, 0 /* lower bound */, 4 /* elements */);
  ScopedSafearray scoped_safe_array(safe_array);
  EXPECT_EQ(scoped_safe_array.Get(), safe_array);
  EXPECT_EQ(scoped_safe_array.Release(), safe_array);
  EXPECT_NE(scoped_safe_array.Receive(), nullptr);

  // The Release() call should have set the internal pointer to nullptr
  EXPECT_EQ(scoped_safe_array.Get(), nullptr);

  scoped_safe_array.Reset(safe_array);
  EXPECT_EQ(scoped_safe_array.Get(), safe_array);

  ScopedSafearray moved_safe_array(std::move(scoped_safe_array));
  EXPECT_EQ(moved_safe_array.Get(), safe_array);
  EXPECT_EQ(moved_safe_array.Release(), safe_array);
  EXPECT_NE(moved_safe_array.Receive(), nullptr);

  // std::move should have cleared the values of scoped_safe_array
  EXPECT_EQ(scoped_safe_array.Get(), nullptr);
  EXPECT_EQ(scoped_safe_array.Release(), nullptr);
  EXPECT_NE(scoped_safe_array.Receive(), nullptr);

  scoped_safe_array.Reset(safe_array);
  EXPECT_EQ(scoped_safe_array.Get(), safe_array);

  ScopedSafearray assigment_moved_safe_array = std::move(scoped_safe_array);
  EXPECT_EQ(assigment_moved_safe_array.Get(), safe_array);
  EXPECT_EQ(assigment_moved_safe_array.Release(), safe_array);
  EXPECT_NE(assigment_moved_safe_array.Receive(), nullptr);

  // The move-assign operator= should have cleared the values of
  // scoped_safe_array
  EXPECT_EQ(scoped_safe_array.Get(), nullptr);
  EXPECT_EQ(scoped_safe_array.Release(), nullptr);
  EXPECT_NE(scoped_safe_array.Receive(), nullptr);

  // Calling Receive() will free the existing reference
  ScopedSafearray safe_array_received(SafeArrayCreateVector(
      VT_R8 /* element type */, 0 /* lower bound */, 4 /* elements */));
  EXPECT_NE(safe_array_received.Receive(), nullptr);
  EXPECT_EQ(safe_array_received.Get(), nullptr);
}

TEST(ScopedSafearrayTest, ScopedSafearrayCast) {
  SAFEARRAY* safe_array = SafeArrayCreateVector(
      VT_R8 /* element type */, 1 /* lower bound */, 5 /* elements */);
  ScopedSafearray scoped_safe_array(safe_array);
  EXPECT_EQ(SafeArrayGetDim(scoped_safe_array.Get()), 1U);

  LONG lower_bound;
  EXPECT_HRESULT_SUCCEEDED(
      SafeArrayGetLBound(scoped_safe_array.Get(), 1, &lower_bound));
  EXPECT_EQ(lower_bound, 1);

  LONG upper_bound;
  EXPECT_HRESULT_SUCCEEDED(
      SafeArrayGetUBound(scoped_safe_array.Get(), 1, &upper_bound));
  EXPECT_EQ(upper_bound, 5);

  VARTYPE variable_type;
  EXPECT_HRESULT_SUCCEEDED(
      SafeArrayGetVartype(scoped_safe_array.Get(), &variable_type));
  EXPECT_EQ(variable_type, VT_R8);
}

}  // namespace win
}  // namespace base

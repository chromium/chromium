// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/is_callback.h"

#include <functional>

#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(CallbackHelpersTest, IsBaseCallback) {
  // Check that base::{Once,Repeating}Closures and references to them are
  // considered base::{Once,Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceClosure>);
  static_assert(base::IsBaseCallback<base::RepeatingClosure>);
  static_assert(base::IsBaseCallback<base::OnceClosure&&>);
  static_assert(base::IsBaseCallback<const base::RepeatingClosure&>);

  // Check that base::{Once, Repeating}Callbacks with a given RunType and
  // references to them are considered base::{Once, Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>>);
  static_assert(base::IsBaseCallback<base::RepeatingCallback<int(int)>>);
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>&&>);
  static_assert(base::IsBaseCallback<const base::RepeatingCallback<int(int)>&>);

  // Check that POD types are not considered base::{Once, Repeating}Callbacks.
  static_assert(!base::IsBaseCallback<bool>);
  static_assert(!base::IsBaseCallback<int>);
  static_assert(!base::IsBaseCallback<double>);

  // Check that the closely related std::function is not considered a
  // base::{Once, Repeating}Callback.
  static_assert(!base::IsBaseCallback<std::function<void()>>);
  static_assert(!base::IsBaseCallback<const std::function<void()>&>);
  static_assert(!base::IsBaseCallback<std::function<void()>&&>);
}

}  // namespace

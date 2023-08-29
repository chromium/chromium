// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/resource_exhaustion.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace base::win {

namespace {

OnResourceExhaustedFunction g_resource_exhausted_function = nullptr;

}  // namespace

void SetOnResourceExhaustedFunction(
    OnResourceExhaustedFunction on_resource_exhausted) {
  g_resource_exhausted_function = on_resource_exhausted;
}

void OnResourceExhausted() {
  // Stop execution here if there is no callback installed.
  CHECK(g_resource_exhausted_function) << "system resource exhausted.";
  g_resource_exhausted_function();
}

}  // namespace base::win

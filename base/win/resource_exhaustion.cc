// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/resource_exhaustion.h"

#include "base/logging.h"

namespace base::win {

namespace {

OnResourceExhaustedFunction g_resource_exhausted_function = nullptr;

}  // namespace

void SetOnResourceExhaustedFunction(
    OnResourceExhaustedFunction on_resource_exhausted) {
  g_resource_exhausted_function = on_resource_exhausted;
}

void OnResourceExhausted() {
  // By default stop execution unless a function has been provided. Code is not
  // assumed to anticipate or handle resource-exhaustion failures. Note that
  // this function is currently intentionally not [[noreturn]]. As of writing
  // chrome/installer/setup/setup_main.cc intentionally continues execution to
  // attempt to propagate the error outwards.
  LOG_IF(FATAL, !g_resource_exhausted_function) << "System resource exhausted.";

  g_resource_exhausted_function();
}

}  // namespace base::win

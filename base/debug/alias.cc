// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/alias.h"

#include "base/compiler_specific.h"

namespace base {
namespace debug {

// This file/function should be excluded from LTO/LTCG to ensure that the
// compiler can't see this function's implementation when compiling calls to it.
NOINLINE void Alias(const void* var) {}

}  // namespace debug
}  // namespace base

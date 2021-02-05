// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/unwinder.h"

namespace base {

void Unwinder::Initialize(ModuleCache* module_cache) {
  module_cache_ = module_cache;
  InitializeModules();
}

}  // namespace base

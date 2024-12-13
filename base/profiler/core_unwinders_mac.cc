// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/core_unwinders.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/profiler/frame_pointer_unwinder.h"

namespace base {

StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  return BindOnce([] {
    std::vector<std::unique_ptr<Unwinder>> unwinders;
    unwinders.push_back(std::make_unique<FramePointerUnwinder>());
    return unwinders;
  });
}

}  // namespace base

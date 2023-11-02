// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_memory_instance.h"

#include "base/callback_helpers.h"

namespace arc {

FakeMemoryInstance::FakeMemoryInstance() = default;
FakeMemoryInstance::~FakeMemoryInstance() = default;

void FakeMemoryInstance::DropCaches(DropCachesCallback callback) {
  std::move(callback).Run(drop_caches_result_);
}

}  // namespace arc

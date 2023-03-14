// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_memory_instance.h"

#include "ash/components/arc/mojom/memory.mojom-forward.h"
#include "ash/components/arc/mojom/memory.mojom-shared.h"

namespace arc {

FakeMemoryInstance::FakeMemoryInstance() = default;
FakeMemoryInstance::~FakeMemoryInstance() = default;

void FakeMemoryInstance::DropCaches(DropCachesCallback callback) {
  std::move(callback).Run(drop_caches_result_);
}

void FakeMemoryInstance::Reclaim(mojom::ReclaimRequestPtr request,
                                 ReclaimCallback callback) {
  if (request->type == mojom::ReclaimType::ANON) {
    std::move(callback).Run(mojom::ReclaimResult::New(
        reclaimed_anon_process_count_, unreclaimed_anon_process_count_));
  } else {
    std::move(callback).Run(mojom::ReclaimResult::New(
        reclaimed_all_process_count_, unreclaimed_all_process_count_));
  }
}
}  // namespace arc

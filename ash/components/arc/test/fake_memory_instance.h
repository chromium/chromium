// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_MEMORY_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_MEMORY_INSTANCE_H_

#include "ash/components/arc/mojom/memory.mojom.h"
#include "base/functional/callback.h"

namespace arc {

class FakeMemoryInstance : public mojom::MemoryInstance {
 public:
  FakeMemoryInstance();
  FakeMemoryInstance(const FakeMemoryInstance&) = delete;
  FakeMemoryInstance& operator=(const FakeMemoryInstance&) = delete;
  ~FakeMemoryInstance() override;

  void set_drop_caches_result(bool result) { drop_caches_result_ = result; }

  void set_reclaim_all_result(uint32_t reclaimed, uint32_t unreclaimed) {
    reclaimed_all_process_count_ = reclaimed;
    unreclaimed_all_process_count_ = unreclaimed;
  }

  void set_reclaim_anon_result(uint32_t reclaimed, uint32_t unreclaimed) {
    reclaimed_anon_process_count_ = reclaimed;
    unreclaimed_anon_process_count_ = unreclaimed;
  }

  // mojom::MemoryInstance:
  void DropCaches(DropCachesCallback callback) override;

  // mojom::MemoryInstance:
  void Reclaim(mojom::ReclaimRequestPtr request,
               ReclaimCallback callback) override;

 private:
  bool drop_caches_result_ = true;
  uint32_t reclaimed_all_process_count_ = 0;
  uint32_t unreclaimed_all_process_count_ = 0;
  uint32_t reclaimed_anon_process_count_ = 0;
  uint32_t unreclaimed_anon_process_count_ = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_MEMORY_INSTANCE_H_

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

  // mojom::MemoryInstance:
  void DropCaches(DropCachesCallback callback) override;

 private:
  bool drop_caches_result_ = true;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_MEMORY_INSTANCE_H_

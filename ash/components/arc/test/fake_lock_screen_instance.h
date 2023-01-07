// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_LOCK_SCREEN_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_LOCK_SCREEN_INSTANCE_H_

#include "ash/components/arc/mojom/lock_screen.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

class FakeLockScreenInstance : public mojom::LockScreenInstance {
 public:
  FakeLockScreenInstance();
  FakeLockScreenInstance(const FakeLockScreenInstance&) = delete;
  FakeLockScreenInstance& operator=(const FakeLockScreenInstance&) = delete;
  ~FakeLockScreenInstance() override;

  // mojom::LockScreenInstance overrides:
  void SetDeviceLocked(bool is_locked) override;

  const absl::optional<bool>& is_locked() const { return is_locked_; }

 private:
  absl::optional<bool> is_locked_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_LOCK_SCREEN_INSTANCE_H_

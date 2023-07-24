// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_HELPER_H_
#define ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_HELPER_H_

#include "ash/ambient/test/test_ambient_client.h"
#include "ash/constants/ash_paths.h"
#include "base/test/scoped_path_override.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"

namespace ash {

// The helper class to test the Ambient Mode in Ash.
class AmbientAshTestHelper {
 public:
  AmbientAshTestHelper();
  ~AmbientAshTestHelper();

  // Simulate to issue an |access_token|.
  // If |is_empty| is true, will return an empty access token.
  void IssueAccessToken(bool is_empty);

  bool IsAccessTokenRequestPending() const;

  device::TestWakeLockProvider* wake_lock_provider() {
    return &wake_lock_provider_;
  }

  TestAmbientClient& ambient_client() { return ambient_client_; }

 private:
  device::TestWakeLockProvider wake_lock_provider_;
  TestAmbientClient ambient_client_{&wake_lock_provider_};

  // Override the screensaver device policy path to be available in tests.
  base::ScopedPathOverride override{ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA};
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_HELPER_H_

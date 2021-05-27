// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_HELPER_H_
#define ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_HELPER_H_

#include <memory>

#include "services/device/public/cpp/test/test_wake_lock_provider.h"

namespace ash {

class TestImageDownloader;
class TestAmbientClient;

// The helper class to test the Ambient Mode in Ash.
class AmbientAshTestHelper {
 public:
  AmbientAshTestHelper();
  ~AmbientAshTestHelper();

  // Simulate to issue an |access_token|.
  // If |with_error| is true, will return an empty access token.
  void IssueAccessToken(const std::string& access_token, bool with_error);

  bool IsAccessTokenRequestPending() const;

  device::TestWakeLockProvider* wake_lock_provider() {
    return &wake_lock_provider_;
  }

 private:
  std::unique_ptr<TestImageDownloader> image_downloader_;
  device::TestWakeLockProvider wake_lock_provider_;
  std::unique_ptr<TestAmbientClient> ambient_client_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_ASH_TEST_HELPER_H_

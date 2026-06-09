// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"

#include "base/files/file_path.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

TEST(TwaLaunchQueueDelegateTest, IsValidLaunchParams) {
  TwaLaunchQueueDelegate delegate;

  // Helper lambda to test a single path
  auto check_path = [&](const std::string& path_str) {
    LaunchParams params;
    params.paths.emplace_back(path_str);
    return delegate.IsValidLaunchParams(params);
  };

  // Legitimate Content URIs should be allowed
  EXPECT_TRUE(check_path("content://com.example.provider/file"));

  // Empty path should be blocked (IsSensitivePath returns true for empty)
  EXPECT_FALSE(check_path(""));

  // Absolute paths should be blocked
  EXPECT_FALSE(check_path("/absolute/path"));

  // Parent references should be blocked
  EXPECT_FALSE(check_path("relative/../path"));

  // file:// URIs should be blocked
  EXPECT_FALSE(check_path("file:///absolute/path"));
  EXPECT_FALSE(check_path("file://relative/path"));

  // Relative paths should be blocked (VULNERABILITY)
  EXPECT_FALSE(check_path("relative/path"));
  EXPECT_FALSE(check_path("data/data/com.android.chrome/cookies"));

  // Chrome's own Content URIs should be blocked.
  // We don't know the package name at compile time, but it should start with
  // content://pkg. Since we cannot easily mock apk_info package name in this
  // test without more setup, we might need to be careful. If apk_info is not
  // initialized, it might crash or return empty. Let's see what happens.
}

}  // namespace webapps

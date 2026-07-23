// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_permission_cache.h"

#include <atomic>
#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/run_until.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_permission_settings {

namespace {

std::atomic<SystemPermission> camera_status_{SystemPermission::kNotDetermined};
std::atomic<SystemPermission> mic_status_{SystemPermission::kNotDetermined};

SystemPermission CheckCamera() {
  return camera_status_;
}
SystemPermission CheckMic() {
  return mic_status_;
}

class SystemMediaPermissionCacheTest : public testing::Test {
 public:
  SystemMediaPermissionCacheTest() = default;
  ~SystemMediaPermissionCacheTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

}  // namespace

// Verifies that the cache correctly starts in an indeterminate state before the
// async refresh completes, and that it successfully updates its cached values
// to reflect the underlying OS state once the thread pool tasks finish.
TEST_F(SystemMediaPermissionCacheTest, InitialStateAndAsyncRefresh) {
  camera_status_ = SystemPermission::kAllowed;
  mic_status_ = SystemPermission::kDenied;

  SystemMediaPermissionCache cache(
      base::BindOnce(&base::ThreadPool::CreateSequencedTaskRunner),
      base::BindRepeating(&CheckCamera), base::BindRepeating(&CheckMic));

  // Initial state before background task runs: NotDetermined.
  EXPECT_TRUE(cache.CanPrompt(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(cache.IsAllowed(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(cache.IsDenied(ContentSettingsType::MEDIASTREAM_CAMERA));

  // Wait for background threadpool tasks to populate cache.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return cache.IsAllowed(ContentSettingsType::MEDIASTREAM_CAMERA) &&
           cache.IsDenied(ContentSettingsType::MEDIASTREAM_MIC);
  }));

  EXPECT_FALSE(cache.CanPrompt(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(cache.IsAllowed(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(cache.CanPrompt(ContentSettingsType::MEDIASTREAM_MIC));
}

// Verifies that `IsDeniedFresh` triggers a new permission check on the worker
// thread, bypassing the stale cached value, and subsequently updates the
// cache with the newly retrieved result.
TEST_F(SystemMediaPermissionCacheTest, IsDeniedFreshBypassesCache) {
  camera_status_ = SystemPermission::kDenied;

  SystemMediaPermissionCache cache(
      base::BindOnce(&base::ThreadPool::CreateSequencedTaskRunner),
      base::BindRepeating(&CheckCamera), base::BindRepeating(&CheckMic));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return cache.IsDenied(ContentSettingsType::MEDIASTREAM_CAMERA);
  }));

  // Update status without refreshing cache.
  camera_status_ = SystemPermission::kAllowed;

  // Cached state is still Denied.
  EXPECT_TRUE(cache.IsDenied(ContentSettingsType::MEDIASTREAM_CAMERA));

  // IsDeniedFresh should query delegate fresh and update cache.
  base::RunLoop run_loop;
  cache.IsDeniedFresh(ContentSettingsType::MEDIASTREAM_CAMERA,
                      base::BindOnce(
                          [](base::RunLoop* run_loop, bool is_denied) {
                            EXPECT_FALSE(is_denied);
                            run_loop->Quit();
                          },
                          &run_loop));
  run_loop.Run();

  // Cache is now updated to Allowed (IsDenied == false, IsAllowed == true).
  EXPECT_FALSE(cache.IsDenied(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(cache.IsAllowed(ContentSettingsType::MEDIASTREAM_CAMERA));
}

}  // namespace system_permission_settings

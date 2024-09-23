// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class OsFeedbackScreenshotManagerTest : public ::testing::Test {
 public:
  OsFeedbackScreenshotManagerTest() = default;
  ~OsFeedbackScreenshotManagerTest() override = default;

 protected:
  void SetTestPngData() {
    OsFeedbackScreenshotManager::GetInstance()->SetPngDataForTesting(
        CreateFakePngData());
  }

  scoped_refptr<base::RefCountedMemory> CreateFakePngData() {
    const unsigned char kData[] = {12, 11, 99};
    return base::MakeRefCounted<base::RefCountedBytes>(kData);
  }
};

// Test that OsFeedbackScreenshotManager is a Singleton.
TEST_F(OsFeedbackScreenshotManagerTest, Singleton) {
  EXPECT_EQ(nullptr, OsFeedbackScreenshotManager::GetIfExists());
  auto* manager = OsFeedbackScreenshotManager::GetInstance();

  EXPECT_EQ(manager, OsFeedbackScreenshotManager::GetInstance());
  EXPECT_EQ(manager, OsFeedbackScreenshotManager::GetIfExists());
}

// Test that DeleteScreenshotData removes the screenshot data.
TEST_F(OsFeedbackScreenshotManagerTest, DeleteScreenshotData) {
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  SetTestPngData();
  EXPECT_NE(nullptr, manager->GetScreenshotData());

  manager->DeleteScreenshotData();

  EXPECT_EQ(nullptr, manager->GetScreenshotData());
}

// Test that TakeScreenshot will skip if a screenshot exists already.
TEST_F(OsFeedbackScreenshotManagerTest, DoNotTakeScreenshotIfExists) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  SetTestPngData();
  EXPECT_NE(nullptr, manager->GetScreenshotData());
  EXPECT_EQ(3u, manager->GetScreenshotData()->size());

  manager->TakeScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(3u, manager->GetScreenshotData()->size());
}

// Test that TakeScreenshot will skip if there is not a window/display.
TEST_F(OsFeedbackScreenshotManagerTest, DoNotTakeScreenshotIfNoWindow) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();

  manager->TakeScreenshot(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

}  // namespace ash

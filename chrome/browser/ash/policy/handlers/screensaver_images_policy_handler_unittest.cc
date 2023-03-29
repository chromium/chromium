// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include <memory>

#include "ash/public/cpp/ambient/ambient_managed_photo_source.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/test/repeating_test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ScreensaverImagesPolicyHandlerTest : public ash::AshTestBase {
 public:
  ScreensaverImagesPolicyHandlerTest() = default;

  ScreensaverImagesPolicyHandlerTest(
      const ScreensaverImagesPolicyHandlerTest&) = delete;
  ScreensaverImagesPolicyHandlerTest& operator=(
      const ScreensaverImagesPolicyHandlerTest&) = delete;

  ~ScreensaverImagesPolicyHandlerTest() override = default;

  void TearDown() override {
    policy_handler_.reset();
    ash::AshTestBase::TearDown();
  }

  void TriggerOnScreensaverImagesDownloaded() {
    ASSERT_TRUE(ScreensaverImagesPolicyHandler::Get());
    policy_handler_->OnScreensaverImagesDownloaded();
  }

  void CreateHandlerInstance() {
    policy_handler_ = std::make_unique<ScreensaverImagesPolicyHandler>();
  }

 private:
  std::unique_ptr<ScreensaverImagesPolicyHandler> policy_handler_;
};

TEST_F(ScreensaverImagesPolicyHandlerTest, SingletonInitialization) {
  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::Get());

  {
    std::unique_ptr<ScreensaverImagesPolicyHandler> handler_instance =
        std::make_unique<ScreensaverImagesPolicyHandler>();

    EXPECT_EQ(handler_instance.get(), ScreensaverImagesPolicyHandler::Get());
  }

  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::Get());
}

TEST_F(ScreensaverImagesPolicyHandlerTest, ShouldRunCallbackIfImagesUpdated) {
  CreateHandlerInstance();
  base::test::RepeatingTestFuture<std::vector<base::FilePath>> test_future;
  ScreensaverImagesPolicyHandler::Get()->SetScreensaverImagesUpdatedCallback(
      test_future.GetCallback<const std::vector<base::FilePath>&>());

  // Expect callbacks when images are downloaded.
  TriggerOnScreensaverImagesDownloaded();
  EXPECT_TRUE(test_future.Wait());
  test_future.Take();
  TriggerOnScreensaverImagesDownloaded();
  EXPECT_TRUE(test_future.Wait());
  test_future.Take();
  EXPECT_TRUE(test_future.IsEmpty());
}

}  // namespace policy

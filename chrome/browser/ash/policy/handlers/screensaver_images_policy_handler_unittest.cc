// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace policy {

class ScreensaverImagesPolicyHandlerTest : public ash::AshTestBase {
 public:
  ScreensaverImagesPolicyHandlerTest() = default;

  ScreensaverImagesPolicyHandlerTest(
      const ScreensaverImagesPolicyHandlerTest&) = delete;
  ScreensaverImagesPolicyHandlerTest& operator=(
      const ScreensaverImagesPolicyHandlerTest&) = delete;

  ~ScreensaverImagesPolicyHandlerTest() override = default;
};

TEST_F(ScreensaverImagesPolicyHandlerTest, SingletonInitialization) {
  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::
                         GetScreensaverImagesPolicyHandlerInstance());

  {
    std::unique_ptr<ScreensaverImagesPolicyHandler> handler_instance =
        std::make_unique<ScreensaverImagesPolicyHandler>();

    EXPECT_EQ(handler_instance.get(),
              ScreensaverImagesPolicyHandler::
                  GetScreensaverImagesPolicyHandlerInstance());
  }

  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::
                         GetScreensaverImagesPolicyHandlerInstance());
}

}  // namespace policy

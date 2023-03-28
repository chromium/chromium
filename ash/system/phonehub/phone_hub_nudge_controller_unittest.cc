// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_nudge_controller.h"
#include "ash/test/ash_test_base.h"

namespace ash {
class PhoneHubNudgeControllerTest : public AshTestBase {
 public:
  PhoneHubNudgeControllerTest() = default;
  PhoneHubNudgeControllerTest(const PhoneHubNudgeControllerTest&) = delete;
  PhoneHubNudgeControllerTest& operator=(const PhoneHubNudgeControllerTest&) =
      delete;
  ~PhoneHubNudgeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<PhoneHubNudgeController>();
  }

  PhoneHubNudgeController* GetController() { return controller_.get(); }

 private:
  std::unique_ptr<PhoneHubNudgeController> controller_;
};

TEST_F(PhoneHubNudgeControllerTest, PhoneHubNudgeControllerExists) {
  PhoneHubNudgeController* controller = GetController();
  ASSERT_TRUE(controller);
}

}  // namespace ash
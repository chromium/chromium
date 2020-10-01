// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/quick_actions_view.h"

#include "ash/system/phonehub/quick_action_item.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

using Status = chromeos::phonehub::TetherController::Status;

namespace {

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

}  // namespace

class QuickActionsViewTest : public AshTestBase {
 public:
  QuickActionsViewTest() = default;
  ~QuickActionsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    quick_actions_view_ =
        std::make_unique<QuickActionsView>(&phone_hub_manager_);
  }

  void TearDown() override {
    quick_actions_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  QuickActionsView* actions_view() { return quick_actions_view_.get(); }
  chromeos::phonehub::FakeTetherController* tether_controller() {
    return phone_hub_manager_.fake_tether_controller();
  }
  chromeos::phonehub::FakeDoNotDisturbController* dnd_controller() {
    return phone_hub_manager_.fake_do_not_disturb_controller();
  }
  chromeos::phonehub::FakeFindMyDeviceController* find_my_device_controller() {
    return phone_hub_manager_.fake_find_my_device_controller();
  }

 private:
  std::unique_ptr<QuickActionsView> quick_actions_view_;
  chromeos::phonehub::FakePhoneHubManager phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(QuickActionsViewTest, QuickActionsToggle) {
  // Initially, silence phone is not enabled.
  EXPECT_FALSE(dnd_controller()->IsDndEnabled());

  // Toggle the button will enable the feature.
  actions_view()->silence_phone_->ButtonPressed(nullptr, DummyEvent());
  EXPECT_TRUE(dnd_controller()->IsDndEnabled());

  // Togge again to disable.
  actions_view()->silence_phone_->ButtonPressed(nullptr, DummyEvent());
  EXPECT_FALSE(dnd_controller()->IsDndEnabled());
}

}  // namespace ash

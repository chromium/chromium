// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/input_device_tracker_impl.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"

namespace ash {

class InputDeviceTrackerTest : public AshTestBase {
 public:
  InputDeviceTrackerTest() = default;
  InputDeviceTrackerTest(const InputDeviceTrackerTest&) = delete;
  InputDeviceTrackerTest& operator=(const InputDeviceTrackerTest&) = delete;
  ~InputDeviceTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    pref_service_ = Shell::Get()->session_controller()->GetActivePrefService();
    tracker_ = std::make_unique<InputDeviceTrackerImpl>(pref_service_);
  }

  void TearDown() override {
    tracker_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceTrackerImpl> tracker_;
  base::raw_ptr<PrefService> pref_service_;
};

TEST_F(InputDeviceTrackerTest, InitializationTest) {
  EXPECT_NE(tracker_.get(), nullptr);
}

}  // namespace ash

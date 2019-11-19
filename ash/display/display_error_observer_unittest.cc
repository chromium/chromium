// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer.h"

#include <memory>

#include "ash/display/display_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {

class DisplayErrorObserverTest : public AshTestBase {
 protected:
  DisplayErrorObserverTest() = default;

  ~DisplayErrorObserverTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = std::make_unique<DisplayErrorObserver>();
  }

 protected:
  DisplayErrorObserver* observer() { return observer_.get(); }

  base::string16 GetMessageContents() {
    return GetDisplayErrorNotificationMessageForTest();
  }

 private:
  std::unique_ptr<DisplayErrorObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorObserverTest);
};

TEST_F(DisplayErrorObserverTest, Normal) {
  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(
      display::DisplayConfigurator::DisplayStateList(),
      display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING),
            GetMessageContents());
}

TEST_F(DisplayErrorObserverTest, CallTwice) {
  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(
      display::DisplayConfigurator::DisplayStateList(),
      display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  base::string16 message = GetMessageContents();
  EXPECT_FALSE(message.empty());

  observer()->OnDisplayModeChangeFailed(
      display::DisplayConfigurator::DisplayStateList(),
      display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  base::string16 message2 = GetMessageContents();
  EXPECT_FALSE(message2.empty());
  EXPECT_EQ(message, message2);
}

TEST_F(DisplayErrorObserverTest, CallWithDifferentState) {
  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(
      display::DisplayConfigurator::DisplayStateList(),
      display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING),
            GetMessageContents());

  observer()->OnDisplayModeChangeFailed(
      display::DisplayConfigurator::DisplayStateList(),
      display::MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_EQ(ui::SubstituteChromeOSDeviceType(
                IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING),
            GetMessageContents());
}

TEST_F(DisplayErrorObserverTest, FailureWithInternalDisplay) {
  // Failure with a single internal display --> No notification.
  UpdateDisplay("200x200,300x300");
  const int64_t internal_display_id = display_manager()->GetDisplayAt(0).id();
  const int64_t external_display_id = display_manager()->GetDisplayAt(1).id();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         internal_display_id);
  auto snapshot1 = display::FakeDisplaySnapshot::Builder()
                       .SetId(internal_display_id)
                       .SetNativeMode({200, 200})
                       .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
                       .Build();
  observer()->OnDisplayModeChangeFailed(
      {snapshot1.get()}, display::MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_TRUE(GetMessageContents().empty());

  // Failure in both displays, user will see a notification even though one of
  // them is the internal display.
  auto snapshot2 = display::FakeDisplaySnapshot::Builder()
                       .SetId(external_display_id)
                       .SetNativeMode({300, 300})
                       .SetType(display::DISPLAY_CONNECTION_TYPE_UNKNOWN)
                       .Build();
  observer()->OnDisplayModeChangeFailed(
      {snapshot1.get(), snapshot2.get()},
      display::MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_EQ(ui::SubstituteChromeOSDeviceType(
                IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING),
            GetMessageContents());
}

}  // namespace ash

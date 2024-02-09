// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/system/bluetooth/hid_preserving_controller/disable_bluetooth_dialog_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/views/test/widget_test.h"

namespace ash {

class DisableBluetoothDialogControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        features::kBluetoothDisconnectWarning);
    disable_bluetooth_dialog_controller_impl_ =
        std::make_unique<DisableBluetoothDialogControllerImpl>();
  }

  void TearDown() override { AshTestBase::TearDown(); }

  const views::View* GetDialogAcceptButton() {
    return disable_bluetooth_dialog_controller_impl_
        ->GetSystemDialogViewForTesting()
        ->GetAcceptButtonForTesting();
  }

  const views::View* GetDialogCancelButton() {
    return disable_bluetooth_dialog_controller_impl_
        ->GetSystemDialogViewForTesting()
        ->GetCancelButtonForTesting();
  }

  void WaitAndAssertWidgetIsDestroyed() {
    views::test::WidgetDestroyedWaiter(
        disable_bluetooth_dialog_controller_impl_->dialog_widget_)
        .Wait();
    ASSERT_FALSE(disable_bluetooth_dialog_controller_impl_->dialog_widget_);
  }

  void ShowDialog(
      const std::vector<std::string>& deviceNames,
      DisableBluetoothDialogController::ShowDialogCallback callback) {
    disable_bluetooth_dialog_controller_impl_->ShowDialog(deviceNames,
                                                          std::move(callback));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<DisableBluetoothDialogControllerImpl>
      disable_bluetooth_dialog_controller_impl_;
};

TEST_F(DisableBluetoothDialogControllerTest, OnAcceptButtonClicked) {
  std::vector<std::string> device_names = {"RGB Keyboard", "MX Keys",
                                           "Logitech SmartMouse"};
  UNCALLED_MOCK_CALLBACK(DisableBluetoothDialogController::ShowDialogCallback,
                         callback);

  ShowDialog(device_names, callback.Get());
  // Click the accept button. `callback` should be called.
  const views::View* const accept_button = GetDialogAcceptButton();
  ASSERT_TRUE(accept_button);
  EXPECT_CALL(callback, Run(true));
  LeftClickOn(accept_button);
  WaitAndAssertWidgetIsDestroyed();
}

TEST_F(DisableBluetoothDialogControllerTest, OnCancelButtonClicked) {
  std::vector<std::string> device_names = {"RGB Keyboard", "MX Keys",
                                           "Logitech SmartMouse"};
  UNCALLED_MOCK_CALLBACK(DisableBluetoothDialogController::ShowDialogCallback,
                         callback);

  ShowDialog(device_names, callback.Get());
  // Click the accept button. `callback` should be called.
  const views::View* const cancel_button = GetDialogCancelButton();
  ASSERT_TRUE(cancel_button);
  EXPECT_CALL(callback, Run(false));
  LeftClickOn(cancel_button);
  WaitAndAssertWidgetIsDestroyed();
}

}  // namespace ash

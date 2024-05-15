// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_UI_CONTROLLER_DELEGATE_H_
#define ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_UI_CONTROLLER_DELEGATE_H_

#include "ash/system/mahi/mahi_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class MahiUiUpdate;
enum class VisibilityState;

// A mock class for testing.
class MockMahiUiControllerDelegate : public MahiUiController::Delegate {
 public:
  explicit MockMahiUiControllerDelegate(MahiUiController* ui_controller);
  MockMahiUiControllerDelegate(const MockMahiUiControllerDelegate&) = delete;
  MockMahiUiControllerDelegate& operator=(const MockMahiUiControllerDelegate&) =
      delete;
  ~MockMahiUiControllerDelegate() override;

  // MahiUiController::Delegate:
  MOCK_METHOD(views::View*, GetView, (), (override));
  MOCK_METHOD(bool, GetViewVisibility, (VisibilityState), (const, override));
  MOCK_METHOD(void, OnUpdated, (const MahiUiUpdate&), (override));
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_UI_CONTROLLER_DELEGATE_H_

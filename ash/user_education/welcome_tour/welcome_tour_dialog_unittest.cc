// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

namespace ash {

using WelcomeTourDialogTest = AshTestBase;

TEST_F(WelcomeTourDialogTest, OnAcceptButtonClicked) {
  base::MockOnceClosure mock_start_tutorial;
  WelcomeTourDialog::CreateAndShow(mock_start_tutorial.Get());
  auto* const welcome_tour_dialog = WelcomeTourDialog::Get();
  ASSERT_TRUE(welcome_tour_dialog);

  // Click the accept button. `mock_start_tutorial` should be called.
  views::View* const accept_button = welcome_tour_dialog->GetViewByID(
      ViewID::VIEW_ID_STYLE_SYSTEM_DIALOG_DELEGATE_ACCEPT_BUTTON);
  ASSERT_TRUE(accept_button);
  EXPECT_CALL(mock_start_tutorial, Run).Times(1);
  LeftClickOn(accept_button);

  // Wait until `welcome_tour_dialog` gets destroyed.
  views::test::WidgetDestroyedWaiter(welcome_tour_dialog->GetWidget()).Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());
}

TEST_F(WelcomeTourDialogTest, OnCancelButtonClicked) {
  base::MockOnceClosure mock_start_tutorial;
  WelcomeTourDialog::CreateAndShow(mock_start_tutorial.Get());
  auto* const welcome_tour_dialog = WelcomeTourDialog::Get();
  ASSERT_TRUE(welcome_tour_dialog);

  // Click the cancel button. `mock_start_tutorial` should NOT be called.
  views::View* const cancel_button = welcome_tour_dialog->GetViewByID(
      ViewID::VIEW_ID_STYLE_SYSTEM_DIALOG_DELEGATE_CANCEL_BUTTON);
  ASSERT_TRUE(cancel_button);
  EXPECT_CALL(mock_start_tutorial, Run).Times(0);
  LeftClickOn(cancel_button);

  // Wait until `welcome_tour_dialog` gets destroyed.
  views::test::WidgetDestroyedWaiter(welcome_tour_dialog->GetWidget()).Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_dialog.h"

#include "ash/constants/ash_features.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/welcome_tour/welcome_tour_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/views/test/widget_test.h"

namespace ash {

// The test suite to check the dialog's features that are independent of the
// Welcome Tour.
class WelcomeTourDialogTest : public UserEducationAshTestBase {
 public:
  WelcomeTourDialogTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWelcomeTour);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WelcomeTourDialogTest, OnAcceptButtonClicked) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, accept_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cancel_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  WelcomeTourDialog::CreateAndShow(accept_callback.Get(), cancel_callback.Get(),
                                   close_callback.Get());
  auto* const welcome_tour_dialog = WelcomeTourDialog::Get();
  ASSERT_TRUE(welcome_tour_dialog);

  // Click the accept button. `accept_callback` should be called.
  const views::View* const accept_button = GetDialogAcceptButton();
  ASSERT_TRUE(accept_button);
  EXPECT_CALL_IN_SCOPE(accept_callback, Run, LeftClickOn(accept_button));

  // Wait until `welcome_tour_dialog` gets destroyed.
  views::test::WidgetDestroyedWaiter(welcome_tour_dialog->GetWidget()).Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());
}

TEST_F(WelcomeTourDialogTest, OnCancelButtonClicked) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, accept_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cancel_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  WelcomeTourDialog::CreateAndShow(accept_callback.Get(), cancel_callback.Get(),
                                   close_callback.Get());
  auto* const welcome_tour_dialog = WelcomeTourDialog::Get();
  ASSERT_TRUE(welcome_tour_dialog);

  // Click the cancel button. `cancel_callback` should be called.
  const views::View* const cancel_button = GetDialogCancelButton();
  ASSERT_TRUE(cancel_button);
  EXPECT_CALL_IN_SCOPE(cancel_callback, Run, LeftClickOn(cancel_button));

  // Wait until `welcome_tour_dialog` gets destroyed.
  views::test::WidgetDestroyedWaiter(welcome_tour_dialog->GetWidget()).Wait();
  EXPECT_FALSE(WelcomeTourDialog::Get());
}

TEST_F(WelcomeTourDialogTest, OnDialogClosedWithoutButtonClicked) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, accept_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cancel_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  WelcomeTourDialog::CreateAndShow(accept_callback.Get(), cancel_callback.Get(),
                                   close_callback.Get());
  auto* const welcome_tour_dialog = WelcomeTourDialog::Get();
  ASSERT_TRUE(welcome_tour_dialog);

  // Close the dialog without clicking any dialog buttons. `close_callback`
  // should be called.
  EXPECT_CALL_IN_SCOPE(close_callback, Run, {
    welcome_tour_dialog->GetWidget()->Close();

    // Wait until `welcome_tour_dialog` gets destroyed.
    views::test::WidgetDestroyedWaiter(welcome_tour_dialog->GetWidget()).Wait();
    EXPECT_FALSE(WelcomeTourDialog::Get());
  });
}

}  // namespace ash

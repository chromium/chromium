// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/disclaimer_view.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

class DisclaimerViewTest : public AshTestBase {
 public:
  DisclaimerViewTest() = default;

  // Mock callbacks:
  MOCK_METHOD(void, OnAcceptButtonPressed, (), ());
  MOCK_METHOD(void, OnDeclineButtonPressed, (), ());

 private:
};

TEST_F(DisclaimerViewTest, AcceptButton) {
  auto widget = CreateFramelessTestWidget();
  widget->SetBounds(gfx::Rect(/*x=*/0, /*y=*/0,
                              /*width=*/1000,
                              /*height=*/800));

  auto* disclaimer_view =
      widget->SetContentsView(std::make_unique<DisclaimerView>(
          base::BindRepeating(&DisclaimerViewTest::OnAcceptButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&DisclaimerViewTest::OnDeclineButtonPressed,
                              base::Unretained(this))));

  EXPECT_CALL(*this, OnAcceptButtonPressed);
  LeftClickOn(disclaimer_view->GetViewByID(kDisclaimerViewAcceptButtonId));
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(DisclaimerViewTest, DeclineButton) {
  auto widget = CreateFramelessTestWidget();
  widget->SetBounds(gfx::Rect(/*x=*/0, /*y=*/0,
                              /*width=*/1000,
                              /*height=*/800));

  auto* disclaimer_view =
      widget->SetContentsView(std::make_unique<DisclaimerView>(
          base::BindRepeating(&DisclaimerViewTest::OnAcceptButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&DisclaimerViewTest::OnDeclineButtonPressed,
                              base::Unretained(this))));

  EXPECT_CALL(*this, OnDeclineButtonPressed);
  LeftClickOn(disclaimer_view->GetViewByID(kDisclaimerViewDeclineButtonId));
  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace ash

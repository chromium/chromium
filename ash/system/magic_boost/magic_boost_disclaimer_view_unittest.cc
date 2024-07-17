// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>

#include "ash/system/magic_boost/magic_boost_constants.h"
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

class MagicBoostDisclaimerViewTest : public AshTestBase {
 public:
  MagicBoostDisclaimerViewTest() = default;

  // Mock callbacks:
  MOCK_METHOD(void, OnAcceptButtonPressed, (), ());
  MOCK_METHOD(void, OnDeclineButtonPressed, (), ());
  MOCK_METHOD(void, OnLinkPressed, (), ());

 private:
};

TEST_F(MagicBoostDisclaimerViewTest, WidgetPosition) {
  UpdateDisplay("1000x800");
  auto* root_window = GetContext();
  auto widget = MagicBoostDisclaimerView::CreateWidget(
      GetPrimaryDisplay().id(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing());

  EXPECT_EQ(widget->GetWindowBoundsInScreen().CenterPoint(),
            root_window->bounds().CenterPoint());
}

TEST_F(MagicBoostDisclaimerViewTest, ButtonActions) {
  auto widget = CreateFramelessTestWidget();
  widget->SetBounds(gfx::Rect(/*x=*/0, /*y=*/0,
                              /*width=*/1000,
                              /*height=*/800));
  auto* disclaimer_view =
      widget->SetContentsView(std::make_unique<MagicBoostDisclaimerView>(
          base::BindRepeating(
              &MagicBoostDisclaimerViewTest::OnAcceptButtonPressed,
              base::Unretained(this)),
          base::BindRepeating(
              &MagicBoostDisclaimerViewTest::OnDeclineButtonPressed,
              base::Unretained(this)),
          base::BindRepeating(&MagicBoostDisclaimerViewTest::OnLinkPressed,
                              base::Unretained(this)),
          base::BindRepeating(&MagicBoostDisclaimerViewTest::OnLinkPressed,
                              base::Unretained(this))));

  auto* accept_button = disclaimer_view->GetViewByID(
      magic_boost::ViewId::DisclaimerViewAcceptButton);

  EXPECT_CALL(*this, OnAcceptButtonPressed);
  LeftClickOn(accept_button);
  testing::Mock::VerifyAndClearExpectations(this);

  auto* decline_button = disclaimer_view->GetViewByID(
      magic_boost::ViewId::DisclaimerViewDeclineButton);

  EXPECT_CALL(*this, OnDeclineButtonPressed);
  LeftClickOn(decline_button);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(MagicBoostDisclaimerViewTest, LinkActions) {
  auto widget = CreateFramelessTestWidget();
  widget->SetBounds(gfx::Rect(/*x=*/0, /*y=*/0,
                              /*width=*/1000,
                              /*height=*/800));
  auto* disclaimer_view =
      widget->SetContentsView(std::make_unique<MagicBoostDisclaimerView>(
          base::BindRepeating(
              &MagicBoostDisclaimerViewTest::OnAcceptButtonPressed,
              base::Unretained(this)),
          base::BindRepeating(
              &MagicBoostDisclaimerViewTest::OnDeclineButtonPressed,
              base::Unretained(this)),
          base::BindRepeating(&MagicBoostDisclaimerViewTest::OnLinkPressed,
                              base::Unretained(this)),
          base::BindRepeating(&MagicBoostDisclaimerViewTest::OnLinkPressed,
                              base::Unretained(this))));

  auto* paragraph_two =
      AsViewClass<views::StyledLabel>(disclaimer_view->GetViewByID(
          magic_boost::ViewId::DisclaimerViewParagraphTwo));

  EXPECT_CALL(*this, OnLinkPressed);
  paragraph_two->ClickFirstLinkForTesting();
  testing::Mock::VerifyAndClearExpectations(this);

  auto* paragraph_four =
      AsViewClass<views::StyledLabel>(disclaimer_view->GetViewByID(
          magic_boost::ViewId::DisclaimerViewParagraphFour));

  EXPECT_CALL(*this, OnLinkPressed);
  paragraph_four->ClickFirstLinkForTesting();
  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace ash

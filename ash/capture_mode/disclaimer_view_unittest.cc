// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/disclaimer_view.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

class DisclaimerViewTestBase : public AshTestBase {
 public:
  DisclaimerViewTestBase() = default;

  // Mock callbacks:
  MOCK_METHOD(void, OnAcceptButtonPressed, (), ());
  MOCK_METHOD(void, OnDeclineButtonPressed, (), ());
  MOCK_METHOD(void, OnTermsOfServiceLinkPressed, (), ());
  MOCK_METHOD(void, OnLearnMoreLinkPressed, (), ());

  // Creates a widget with a `DisclaimerView` as the contents view and shows it.
  std::unique_ptr<views::Widget> CreateAndShowWidget() {
    auto widget = DisclaimerView::CreateWidget(
        Shell::GetPrimaryRootWindow(), is_reminder(),
        base::BindRepeating(&DisclaimerViewTestBase::OnAcceptButtonPressed,
                            base::Unretained(this)),
        base::BindRepeating(&DisclaimerViewTestBase::OnDeclineButtonPressed,
                            base::Unretained(this)),
        base::BindRepeating(
            &DisclaimerViewTestBase::OnTermsOfServiceLinkPressed,
            base::Unretained(this)),
        base::BindRepeating(&DisclaimerViewTestBase::OnLearnMoreLinkPressed,
                            base::Unretained(this)));
    widget->Show();
    views::test::WidgetVisibleWaiter(widget.get()).Wait();
    return widget;
  }

  virtual bool is_reminder() = 0;

 private:
};

class DisclaimerViewTest : public DisclaimerViewTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  bool is_reminder() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         DisclaimerViewTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Reminder" : "NoReminder";
                         });

class DisclaimerViewNoReminderTest : public DisclaimerViewTestBase {
 public:
  bool is_reminder() override { return false; }
};

class DisclaimerViewReminderTest : public DisclaimerViewTestBase {
 public:
  bool is_reminder() override { return true; }
};

TEST_P(DisclaimerViewTest, AcceptButton) {
  std::unique_ptr<views::Widget> disclaimer_widget = CreateAndShowWidget();

  EXPECT_CALL(*this, OnAcceptButtonPressed);
  LeftClickOn(disclaimer_widget->GetContentsView()->GetViewByID(
      kDisclaimerViewAcceptButtonId));
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_P(DisclaimerViewTest, AcceptButtonKeyboardNavigation) {
  std::unique_ptr<views::Widget> disclaimer_widget = CreateAndShowWidget();

  EXPECT_CALL(*this, OnAcceptButtonPressed);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(DisclaimerViewNoReminderTest, DeclineButton) {
  std::unique_ptr<views::Widget> disclaimer_widget = CreateAndShowWidget();

  EXPECT_CALL(*this, OnDeclineButtonPressed);
  LeftClickOn(disclaimer_widget->GetContentsView()->GetViewByID(
      kDisclaimerViewDeclineButtonId));
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(DisclaimerViewNoReminderTest, DeclineButtonKeyboardNavigation) {
  std::unique_ptr<views::Widget> disclaimer_widget = CreateAndShowWidget();

  EXPECT_CALL(*this, OnDeclineButtonPressed);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(DisclaimerViewReminderTest, DeclineButtonHidden) {
  std::unique_ptr<views::Widget> disclaimer_widget = CreateAndShowWidget();

  EXPECT_FALSE(disclaimer_widget->GetContentsView()->GetViewByID(
      kDisclaimerViewDeclineButtonId));
}

TEST_P(DisclaimerViewTest, TermsOfServiceLink) {
  std::unique_ptr<views::Widget> widget = CreateAndShowWidget();
  auto* disclaimer_view =
      views::AsViewClass<DisclaimerView>(widget->GetContentsView());

  auto* paragraph = views::AsViewClass<views::StyledLabel>(
      disclaimer_view->GetViewByID(kDisclaimerViewParagraphOneId));
  ASSERT_TRUE(paragraph);
  views::Link* link = paragraph->GetFirstLinkForTesting();
  ASSERT_TRUE(link);
  EXPECT_CALL(*this, OnTermsOfServiceLinkPressed);
  LeftClickOn(link);
}

TEST_P(DisclaimerViewTest, LearnMoreLink) {
  std::unique_ptr<views::Widget> widget = CreateAndShowWidget();
  auto* disclaimer_view =
      views::AsViewClass<DisclaimerView>(widget->GetContentsView());

  auto* paragraph = views::AsViewClass<views::StyledLabel>(
      disclaimer_view->GetViewByID(kDisclaimerViewParagraphThreeId));
  ASSERT_TRUE(paragraph);
  views::Link* link = paragraph->GetFirstLinkForTesting();
  ASSERT_TRUE(link);
  EXPECT_CALL(*this, OnLearnMoreLinkPressed);
  LeftClickOn(link);
}

}  // namespace ash

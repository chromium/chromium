// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_controller_impl.h"

#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/quick_answers/ui/quick_answers_view.h"
#include "ash/quick_answers/ui/user_notice_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_notice.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/network/test/test_url_loader_factory.h"

namespace ash {

namespace {

constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));
constexpr char kDefaultTitle[] = "default_title";

gfx::Rect BoundsWithXPosition(int x) {
  constexpr int kAnyValue = 100;
  return gfx::Rect(x, /*y=*/kAnyValue, /*width=*/kAnyValue,
                   /*height=*/kAnyValue);
}

}  // namespace

class QuickAnswersControllerTest : public AshTestBase {
 protected:
  QuickAnswersControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kQuickAnswers);
  }
  QuickAnswersControllerTest(const QuickAnswersControllerTest&) = delete;
  QuickAnswersControllerTest& operator=(const QuickAnswersControllerTest&) =
      delete;
  ~QuickAnswersControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller()->SetClient(
        std::make_unique<chromeos::quick_answers::QuickAnswersClient>(
            &test_url_loader_factory_, ash::AssistantState::Get(),
            controller()->GetQuickAnswersDelegate()));

    controller()->OnEligibilityChanged(true);
    controller()->SetVisibilityForTesting(QuickAnswersVisibility::kPending);
  }

  QuickAnswersControllerImpl* controller() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  // Show the quick answer or notice view (depending on the notice
  // consent status).
  void ShowView(bool set_visibility = true) {
    // To show the quick answers view, its visibility must be set to 'pending'
    // first.
    if (set_visibility)
      controller()->SetPendingShowQuickAnswers();
    controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                        kDefaultTitle, {});
  }

  void ShowNoticeView() {
    // We can only show the notice view if the consent has not been
    // granted, so we add a sanity check here.
    EXPECT_TRUE(notice_controller()->ShouldShowNotice())
        << "Can not show notice view as the user consent has already "
           "been given.";
    ShowView();
  }

  void ShowQuickAnswersView() {
    // Grant the user consent so the quick answers view is shown.
    AcceptNotice();
    ShowView();
  }

  const views::View* GetQuickAnswersView() {
    return ui_controller()->quick_answers_view_for_testing();
  }

  const views::View* GetNoticeView() {
    return ui_controller()->notice_view_for_testing();
  }

  void AcceptNotice() {
    notice_controller()->StartNotice();
    notice_controller()->AcceptNotice(
        chromeos::quick_answers::NoticeInteractionType::kAccept);
  }

  void DismissQuickAnswers() {
    controller()->DismissQuickAnswers(/*is_active=*/true);
  }

  QuickAnswersUiController* ui_controller() {
    return controller()->quick_answers_ui_controller();
  }

  chromeos::quick_answers::QuickAnswersNotice* notice_controller() {
    return controller()->GetNoticeControllerForTesting();
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(QuickAnswersControllerTest, ShouldNotShowWhenFeatureNotEligible) {
  controller()->OnEligibilityChanged(false);
  ShowView();

  // The feature is not eligible, nothing should be shown.
  EXPECT_FALSE(ui_controller()->is_showing_user_notice_view());
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
}

TEST_F(QuickAnswersControllerTest, ShouldNotShowWhenClosed) {
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kClosed);
  ShowView(/*set_visibility=*/false);

  // The UI is closed and session is inactive, nothing should be shown.
  EXPECT_FALSE(ui_controller()->is_showing_user_notice_view());
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest,
       ShouldShowPendingQueryAfterUserAcceptsNotice) {
  ShowView();
  // Without user consent, only the user consent view should show.
  EXPECT_TRUE(ui_controller()->is_showing_user_notice_view());
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());

  controller()->OnUserNoticeAccepted();

  // With user consent granted, the consent view should dismiss and the cached
  // quick answer query should show.
  EXPECT_FALSE(ui_controller()->is_showing_user_notice_view());
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kVisible);
}

TEST_F(QuickAnswersControllerTest, UserNoticeAlreadyAccepted) {
  AcceptNotice();
  ShowView();

  // With user consent already accepted, only the quick answers view should
  // show.
  EXPECT_FALSE(ui_controller()->is_showing_user_notice_view());
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kVisible);
}

TEST_F(QuickAnswersControllerTest,
       ShouldShowQuickAnswersIfUserIgnoresNoticeViewThreeTimes) {
  // Show and dismiss user consent window the first 3 times
  for (int i = 0; i < 3; i++) {
    ShowView();
    EXPECT_TRUE(ui_controller()->is_showing_user_notice_view())
        << "Notice view not shown the " << (i + 1) << " time";
    EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
    DismissQuickAnswers();
  }

  // The 4th time we should simply show the quick answer.
  ShowView();
  EXPECT_FALSE(ui_controller()->is_showing_user_notice_view());
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());
}

TEST_F(QuickAnswersControllerTest, DismissUserNoticeView) {
  ShowNoticeView();
  EXPECT_TRUE(ui_controller()->is_showing_user_notice_view());

  DismissQuickAnswers();

  EXPECT_FALSE(ui_controller()->is_showing_user_notice_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest, DismissQuickAnswersView) {
  ShowQuickAnswersView();
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());

  controller()->DismissQuickAnswers(true);
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest,
       ShouldUpdateQuickAnswersViewBoundsWhenMenuBoundsChange) {
  ShowQuickAnswersView();

  controller()->UpdateQuickAnswersAnchorBounds(BoundsWithXPosition(123));

  // We only check the 'x' position as that is guaranteed to be identical
  // between the view and the menu.
  const views::View* quick_answers_view = GetQuickAnswersView();
  EXPECT_EQ(123, quick_answers_view->GetBoundsInScreen().x());
}

TEST_F(QuickAnswersControllerTest,
       ShouldUpdateNoticeViewBoundsWhenMenuBoundsChange) {
  ShowNoticeView();

  controller()->UpdateQuickAnswersAnchorBounds(BoundsWithXPosition(123));

  // We only check the 'x' position as that is guaranteed to be identical
  // between the view and the menu.
  const views::View* notice_view = GetNoticeView();
  EXPECT_EQ(123, notice_view->GetBoundsInScreen().x());
}

}  // namespace ash

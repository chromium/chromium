// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/quick_answers_view.h"

#include "ash/quick_answers/quick_answers_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kMarginDip = 10;
constexpr int kSmallTop = 30;
constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));

}  // namespace

class QuickAnswersViewsTest : public AshTestBase {
 protected:
  QuickAnswersViewsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kQuickAnswers);
  }
  QuickAnswersViewsTest(const QuickAnswersViewsTest&) = delete;
  QuickAnswersViewsTest& operator=(const QuickAnswersViewsTest&) = delete;
  ~QuickAnswersViewsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    anchor_bounds_ = kDefaultAnchorBoundsInScreen;
    CreateQuickAnswersView(anchor_bounds_, "default_title",
                           /*create_menu=*/false);
  }

  void TearDown() override {
    quick_answers_view_.reset();

    // Menu.
    menu_parent_.reset();
    menu_runner_.reset();
    menu_model_.reset();
    menu_delegate_.reset();

    AshTestBase::TearDown();
  }

  // Currently instantiated QuickAnswersView instance.
  QuickAnswersView* view() { return quick_answers_view_.get(); }

  // Needed to poll the current bounds of the mock anchor.
  const gfx::Rect& GetAnchorBounds() { return anchor_bounds_; }

  // Create a QuickAnswersView instance with custom anchor-bounds and
  // title-text.
  void CreateQuickAnswersView(const gfx::Rect anchor_bounds,
                              const char* title,
                              bool create_menu) {
    // Reset existing view if any.
    quick_answers_view_.reset();

    // Set up a companion menu before creating the QuickAnswersView.
    if (create_menu)
      CreateAndShowBasicMenu();

    anchor_bounds_ = anchor_bounds;
    auto* ui_controller =
        static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get())
            ->quick_answers_ui_controller();
    quick_answers_view_ = std::make_unique<QuickAnswersView>(
        anchor_bounds_, title, ui_controller);
  }

  void CreateAndShowBasicMenu() {
    menu_delegate_ = std::make_unique<views::Label>();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(menu_delegate_.get());
    menu_model_->AddItem(0, base::ASCIIToUTF16("Menu item"));
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
    menu_parent_ = CreateTestWidget();
    menu_runner_->RunMenuAt(menu_parent_.get(), nullptr, gfx::Rect(),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_MOUSE);
  }

 private:
  std::unique_ptr<QuickAnswersView> quick_answers_view_;
  gfx::Rect anchor_bounds_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Menu.
  std::unique_ptr<views::Label> menu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<views::Widget> menu_parent_;
};

TEST_F(QuickAnswersViewsTest, DefaultLayoutAroundAnchor) {
  gfx::Rect view_bounds = view()->GetBoundsInScreen();
  gfx::Rect anchor_bounds = GetAnchorBounds();

  // Vertically aligned with anchor.
  EXPECT_EQ(view_bounds.x(), anchor_bounds.x());
  EXPECT_EQ(view_bounds.right(), anchor_bounds.right());

  // View is positioned above the anchor.
  EXPECT_EQ(view_bounds.bottom() + kMarginDip, anchor_bounds.y());
}

TEST_F(QuickAnswersViewsTest, PositionedBelowAnchorIfLessSpaceAbove) {
  gfx::Rect anchor_bounds = GetAnchorBounds();
  // Update anchor-bounds' position so that it does not leave enough vertical
  // space above it to show the QuickAnswersView.
  anchor_bounds.set_y(kSmallTop);

  CreateQuickAnswersView(anchor_bounds, "title", /*create_menu=*/false);
  gfx::Rect view_bounds = view()->GetBoundsInScreen();

  // Anchor is positioned above the view.
  EXPECT_EQ(anchor_bounds.bottom() + kMarginDip, view_bounds.y());
}

TEST_F(QuickAnswersViewsTest, FocusProperties) {
  // Not focused by default.
  EXPECT_FALSE(view()->HasFocus());

  // Does not gain focus upon request if no active menu.
  CHECK(views::MenuController::GetActiveInstance() == nullptr);
  view()->RequestFocus();
  EXPECT_FALSE(view()->HasFocus());

  // Set up a companion menu before creating a new view.
  CreateQuickAnswersView(GetAnchorBounds(), "title",
                         /*create_menu=*/true);
  CHECK(views::MenuController::GetActiveInstance() &&
        views::MenuController::GetActiveInstance()->owner());

  // Gains focus only upon request, if an owned menu was active when the view
  // was created.
  CHECK(views::MenuController::GetActiveInstance() != nullptr);
  EXPECT_FALSE(view()->HasFocus());
  view()->RequestFocus();
  EXPECT_TRUE(view()->HasFocus());
}

}  // namespace ash

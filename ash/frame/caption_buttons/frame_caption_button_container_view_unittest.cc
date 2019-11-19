// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/test_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace ash {

namespace {

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate(bool can_maximize,
                     bool can_minimize,
                     bool close_button_visible)
      : can_maximize_(can_maximize),
        can_minimize_(can_minimize),
        close_button_visible_(close_button_visible) {}
  ~TestWidgetDelegate() override = default;

  bool CanMaximize() const override { return can_maximize_; }

  bool CanMinimize() const override { return can_minimize_; }

  bool ShouldShowCloseButton() const override { return close_button_visible_; }

 private:
  bool can_maximize_;
  bool can_minimize_;
  bool close_button_visible_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

class FrameCaptionButtonContainerViewTest : public AshTestBase {
 public:
  enum MaximizeAllowed { MAXIMIZE_ALLOWED, MAXIMIZE_DISALLOWED };

  enum MinimizeAllowed { MINIMIZE_ALLOWED, MINIMIZE_DISALLOWED };

  enum CloseButtonVisible { CLOSE_BUTTON_VISIBLE, CLOSE_BUTTON_NOT_VISIBLE };

  FrameCaptionButtonContainerViewTest() = default;

  ~FrameCaptionButtonContainerViewTest() override = default;

  // Creates a widget which allows maximizing based on |maximize_allowed|.
  // The caller takes ownership of the returned widget.
  views::Widget* CreateTestWidget(MaximizeAllowed maximize_allowed,
                                  MinimizeAllowed minimize_allowed,
                                  CloseButtonVisible close_button_visible)
      WARN_UNUSED_RESULT {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params;
    params.delegate =
        new TestWidgetDelegate(maximize_allowed == MAXIMIZE_ALLOWED,
                               minimize_allowed == MINIMIZE_ALLOWED,
                               close_button_visible == CLOSE_BUTTON_VISIBLE);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(std::move(params));
    return widget;
  }

  // Sets arbitrary images for the icons and assign the default caption button
  // size to the buttons in |container|.
  void InitContainer(FrameCaptionButtonContainerView* container) {
    container->SetButtonSize(views::GetCaptionButtonLayoutSize(
        views::CaptionButtonLayoutSize::kNonBrowserCaption));
    for (int icon = 0; icon < views::CAPTION_BUTTON_ICON_COUNT; ++icon) {
      container->SetButtonImage(static_cast<views::CaptionButtonIcon>(icon),
                                views::kWindowControlCloseIcon);
    }
    container->SizeToPreferredSize();
  }

  // Tests that |leftmost| and |rightmost| are at |container|'s edges.
  bool CheckButtonsAtEdges(FrameCaptionButtonContainerView* container,
                           const views::FrameCaptionButton& leftmost,
                           const views::FrameCaptionButton& rightmost) {
    gfx::Rect expected(container->GetPreferredSize());

    gfx::Rect container_size(container->GetPreferredSize());
    if (leftmost.y() == rightmost.y() &&
        leftmost.height() == rightmost.height() &&
        leftmost.x() == expected.x() && leftmost.y() == expected.y() &&
        leftmost.height() == expected.height() &&
        rightmost.bounds().right() == expected.right()) {
      return true;
    }

    LOG(ERROR) << "Buttons " << leftmost.bounds().ToString() << " "
               << rightmost.bounds().ToString() << " not at edges of "
               << expected.ToString();
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButtonContainerViewTest);
};

// Test how the allowed actions affect which caption buttons are visible.
TEST_F(FrameCaptionButtonContainerViewTest, ButtonVisibility) {
  // All the buttons should be visible when minimizing and maximizing are
  // allowed.
  FrameCaptionButtonContainerView container1(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container1);
  container1.Layout();
  FrameCaptionButtonContainerView::TestApi t1(&container1);
  EXPECT_TRUE(t1.minimize_button()->GetVisible());
  EXPECT_TRUE(t1.size_button()->GetVisible());
  EXPECT_TRUE(t1.close_button()->GetVisible());
  EXPECT_TRUE(CheckButtonsAtEdges(&container1, *t1.minimize_button(),
                                  *t1.close_button()));

  // The minimize button should be visible when minimizing is allowed but
  // maximizing is disallowed.
  FrameCaptionButtonContainerView container2(CreateTestWidget(
      MAXIMIZE_DISALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container2);
  container2.Layout();
  FrameCaptionButtonContainerView::TestApi t2(&container2);
  EXPECT_TRUE(t2.minimize_button()->GetVisible());
  EXPECT_FALSE(t2.size_button()->GetVisible());
  EXPECT_TRUE(t2.close_button()->GetVisible());
  EXPECT_TRUE(CheckButtonsAtEdges(&container2, *t2.minimize_button(),
                                  *t2.close_button()));

  // Neither the minimize button nor the size button should be visible when
  // neither minimizing nor maximizing are allowed.
  FrameCaptionButtonContainerView container3(CreateTestWidget(
      MAXIMIZE_DISALLOWED, MINIMIZE_DISALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container3);
  container3.Layout();
  FrameCaptionButtonContainerView::TestApi t3(&container3);
  EXPECT_FALSE(t3.minimize_button()->GetVisible());
  EXPECT_FALSE(t3.size_button()->GetVisible());
  EXPECT_TRUE(t3.close_button()->GetVisible());
  EXPECT_TRUE(
      CheckButtonsAtEdges(&container3, *t3.close_button(), *t3.close_button()));
}

// Tests that the layout animations trigered by button visibility result in the
// correct placement of the buttons.
TEST_F(FrameCaptionButtonContainerViewTest,
       TestUpdateSizeButtonVisibilityAnimation) {
  FrameCaptionButtonContainerView container(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));

  // Add an extra button to the left of the size button to verify that it is
  // repositioned similarly to the minimize button. This simulates the PWA menu
  // button being added to the left of the minimize button.
  views::View* extra_button = new views::StaticSizedView(gfx::Size(32, 32));
  container.AddChildViewAt(extra_button, 0);

  InitContainer(&container);
  container.Layout();

  FrameCaptionButtonContainerView::TestApi test(&container);
  gfx::Rect initial_extra_button_bounds = extra_button->bounds();
  gfx::Rect initial_minimize_button_bounds = test.minimize_button()->bounds();
  gfx::Rect initial_size_button_bounds = test.size_button()->bounds();
  gfx::Rect initial_close_button_bounds = test.close_button()->bounds();
  gfx::Rect initial_container_bounds = container.bounds();

  ASSERT_EQ(initial_minimize_button_bounds.x(),
            initial_extra_button_bounds.right());
  ASSERT_EQ(initial_size_button_bounds.x(),
            initial_minimize_button_bounds.right());
  ASSERT_EQ(initial_close_button_bounds.x(),
            initial_size_button_bounds.right());

  // Button positions should be the same when entering tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  container.UpdateCaptionButtonState(false /*=animate*/);
  test.EndAnimations();
  // Parent needs to layout in response to size change.
  container.Layout();

  EXPECT_TRUE(test.minimize_button()->GetVisible());
  EXPECT_TRUE(test.size_button()->GetVisible());
  EXPECT_TRUE(test.close_button()->GetVisible());
  gfx::Rect extra_button_bounds = extra_button->bounds();
  gfx::Rect minimize_button_bounds = test.minimize_button()->bounds();
  gfx::Rect size_button_bounds = test.size_button()->bounds();
  gfx::Rect close_button_bounds = test.close_button()->bounds();
  EXPECT_EQ(minimize_button_bounds.x(), extra_button_bounds.right());
  EXPECT_EQ(size_button_bounds.x(), minimize_button_bounds.right());
  EXPECT_EQ(close_button_bounds.x(), size_button_bounds.right());
  EXPECT_EQ(initial_size_button_bounds, test.size_button()->bounds());
  EXPECT_EQ(initial_close_button_bounds.size(), close_button_bounds.size());
  EXPECT_EQ(container.GetPreferredSize().width(),
            initial_container_bounds.width());

  // Button positions should be the same when leaving tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  container.UpdateCaptionButtonState(false /*=animate*/);
  // Calling code needs to layout in response to size change.
  container.Layout();
  test.EndAnimations();
  EXPECT_TRUE(test.minimize_button()->GetVisible());
  EXPECT_TRUE(test.size_button()->GetVisible());
  EXPECT_TRUE(test.close_button()->GetVisible());
  EXPECT_EQ(initial_extra_button_bounds, extra_button->bounds());
  EXPECT_EQ(initial_minimize_button_bounds, test.minimize_button()->bounds());
  EXPECT_EQ(initial_size_button_bounds, test.size_button()->bounds());
  EXPECT_EQ(initial_close_button_bounds, test.close_button()->bounds());
  EXPECT_EQ(container.GetPreferredSize().width(),
            initial_container_bounds.width());
}

// Test that the close button is visible when
// |WidgetDelegate::ShouldShowCloseButton()| returns true.
TEST_F(FrameCaptionButtonContainerViewTest, ShouldShowCloseButtonTrue) {
  FrameCaptionButtonContainerView container(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container);
  container.Layout();
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_TRUE(testApi.close_button()->GetVisible());
  EXPECT_TRUE(testApi.close_button()->GetEnabled());
}

// Test that the close button is not visible when
// |WidgetDelegate::ShouldShowCloseButton()| returns false.
TEST_F(FrameCaptionButtonContainerViewTest, ShouldShowCloseButtonFalse) {
  FrameCaptionButtonContainerView container(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_NOT_VISIBLE));
  InitContainer(&container);
  container.Layout();
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_FALSE(testApi.close_button()->GetVisible());
  EXPECT_TRUE(testApi.close_button()->GetEnabled());
}

}  // namespace ash

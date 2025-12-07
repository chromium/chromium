// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_input_protector.h"

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metrics.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

class TestDialogDelegate : public views::DialogDelegate {
 public:
  TestDialogDelegate() = default;

  TestDialogDelegate(const TestDialogDelegate&) = delete;
  TestDialogDelegate& operator=(const TestDialogDelegate&) = delete;
  ~TestDialogDelegate() override = default;

  // DialogDelegate overrides:
  bool ShouldIgnoreButtonPressedEventHandling(
      views::View* button,
      const ui::Event& event) const override {
    return input_protector_ && input_protector_->OccludedByPictureInPicture();
  }
  void OnWidgetInitialized() override {
    input_protector_ = std::make_unique<PictureInPictureInputProtector>(this);
  }

 private:
  std::unique_ptr<PictureInPictureInputProtector> input_protector_;
};

class PictureInPictureInputProtectorTest : public ChromeViewsTestBase {
 public:
  PictureInPictureInputProtectorTest() = default;
  PictureInPictureInputProtectorTest(
      const PictureInPictureInputProtectorTest&) = delete;
  PictureInPictureInputProtectorTest& operator=(
      const PictureInPictureInputProtectorTest&) = delete;
  ~PictureInPictureInputProtectorTest() override = default;

 protected:
  std::unique_ptr<views::Widget> CreatePictureInPictureWidget() {
    std::unique_ptr<views::Widget> picture_in_picture_widget =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    picture_in_picture_widget->Show();
    picture_in_picture_widget->SetBounds({0, 0, 200, 200});
    PictureInPictureWindowManager::GetInstance()
        ->GetOcclusionTracker()
        ->OnPictureInPictureWidgetOpened(picture_in_picture_widget.get());
    return picture_in_picture_widget;
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    picture_in_picture_widget_ = CreatePictureInPictureWidget();

    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();

    dialog_delegate_ = std::make_unique<TestDialogDelegate>();

    widget_to_protect_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = dialog_delegate_.get();
    params.parent = parent_widget_->GetNativeView();
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    widget_to_protect_->Init(std::move(params));
    widget_to_protect_->SetBounds({300, 0, 200, 200});
    widget_to_protect_->Show();
  }

  void TearDown() override {
    picture_in_picture_widget_.reset();
    widget_to_protect_.reset();
    dialog_delegate_.reset();
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestDialogDelegate* dialog_delegate() { return dialog_delegate_.get(); }
  views::Widget* widget_to_protect() { return widget_to_protect_.get(); }
  views::Widget* picture_in_picture_widget() {
    return picture_in_picture_widget_.get();
  }

  bool WidgetsOverlap() {
    return picture_in_picture_widget()->GetWindowBoundsInScreen().Intersects(
        widget_to_protect()->GetWindowBoundsInScreen());
  }

  void WaitForBoundsChangedDebounce() {
    task_environment()->FastForwardBy(base::Milliseconds(300));
  }

 private:
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<TestDialogDelegate> dialog_delegate_;
  std::unique_ptr<views::Widget> widget_to_protect_;
  std::unique_ptr<views::Widget> picture_in_picture_widget_;
};

TEST_F(PictureInPictureInputProtectorTest,
       AllowsKeyEventsWhenWidgetsDoNotOverlap) {
  // Verify that the widgets do not overlap.
  ASSERT_FALSE(WidgetsOverlap());

  // Create a dummy button and ui event.
  views::View dummy_button;
  ui::KeyEvent dummy_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE, ui::EventTimeForNow());

  // Verify that the dialog delegate does not ignore button pressed event
  // handling, since the widgets do not overlap.
  EXPECT_FALSE(dialog_delegate()->ShouldIgnoreButtonPressedEventHandling(
      &dummy_button, dummy_event));
}

TEST_F(PictureInPictureInputProtectorTest, BlocksKeyEventsWhenWidgetsOverlap) {
  // Verify that the widgets do not overlap.
  ASSERT_FALSE(WidgetsOverlap());

  // Create a dummy button and ui event.
  views::View dummy_button;
  ui::KeyEvent dummy_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE, ui::EventTimeForNow());

  // Verify that the dialog delegate does not ignore button pressed event
  // handling, since the widgets do not overlap.
  EXPECT_FALSE(dialog_delegate()->ShouldIgnoreButtonPressedEventHandling(
      &dummy_button, dummy_event));

  // Move the widget to protect so it overlaps with the picture-in-picture
  // widget.
  widget_to_protect()->SetBounds(
      picture_in_picture_widget()->GetWindowBoundsInScreen());
  WaitForBoundsChangedDebounce();

  // Verify that the widgets overlap.
  ASSERT_TRUE(WidgetsOverlap());

  // Verify that the dialog delegate ignores button pressed event handling,
  // since the widgets overlap.
  EXPECT_TRUE(dialog_delegate()->ShouldIgnoreButtonPressedEventHandling(
      &dummy_button, dummy_event));
}

}  // namespace

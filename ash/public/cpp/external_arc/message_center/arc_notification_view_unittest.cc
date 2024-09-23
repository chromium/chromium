// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"

#include <memory>

#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_surface.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/shell.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  TestTextInputClient() : ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  TestTextInputClient(const TestTextInputClient&) = delete;
  TestTextInputClient& operator=(const TestTextInputClient&) = delete;

  ui::TextInputType GetTextInputType() const override { return type_; }

  void set_text_input_type(ui::TextInputType type) { type_ = type; }

 private:
  ui::TextInputType type_ = ui::TEXT_INPUT_TYPE_NONE;
};

}  // namespace

class ArcNotificationViewTest : public AshTestBase {
 public:
  ArcNotificationViewTest() = default;

  ArcNotificationViewTest(const ArcNotificationViewTest&) = delete;
  ArcNotificationViewTest& operator=(const ArcNotificationViewTest&) = delete;

  ~ArcNotificationViewTest() override = default;

  // views::ViewsTestBase
  void SetUp() override {
    AshTestBase::SetUp();

    item_ = std::make_unique<MockArcNotificationItem>(kDefaultNotificationKey);

    MessageViewFactory::ClearCustomNotificationViewFactory(
        kArcNotificationCustomViewType);
    MessageViewFactory::SetCustomNotificationViewFactory(
        kArcNotificationCustomViewType,
        base::BindRepeating(
            &ArcNotificationViewTest::CreateCustomMessageViewForTest,
            base::Unretained(this), item_.get()));

    std::unique_ptr<Notification> notification = CreateSimpleNotification();

    std::unique_ptr<ArcNotificationView> notification_view(
        static_cast<ArcNotificationView*>(
            MessageViewFactory::Create(*notification, /*shown_in_popup=*/false)
                .release()));
    notification_view_ = notification_view.get();
    surface_ =
        std::make_unique<MockArcNotificationSurface>(kDefaultNotificationKey);
    notification_view_->content_view_->SetSurface(surface_.get());
    UpdateNotificationViews(*notification);

    views::Widget::InitParams init_params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.context = GetContext();
    init_params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        desks_util::GetActiveDeskContainerId());
    views::Widget* widget = new views::Widget();
    widget->Init(std::move(init_params));
    widget->SetContentsView(std::move(notification_view));
    widget->SetSize(notification_view_->GetPreferredSize());
    widget->Show();
    EXPECT_EQ(widget, notification_view_->GetWidget());
  }

  std::unique_ptr<Notification> CreateSimpleNotification() {
    std::unique_ptr<Notification> notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_CUSTOM, kDefaultNotificationId,
        u"title", u"message", ui::ImageModel(), u"display source", GURL(),
        message_center::NotifierId(
            message_center::NotifierType::ARC_APPLICATION, "test_app_id"),
        message_center::RichNotificationData(), nullptr);

    notification->set_custom_view_type(kArcNotificationCustomViewType);
    return notification;
  }

  void TearDown() override {
    widget()->Close();
    item_.reset();
    notification_.reset();
    surface_.reset();
    AshTestBase::TearDown();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::EventType::kMousePressed, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::EventType::kMouseReleased, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&released_event);
  }

  void PerformKeyEvents(ui::KeyboardCode code) {
    ui::KeyEvent event1 =
        ui::KeyEvent(ui::EventType::kKeyPressed, code, ui::EF_NONE);
    widget()->OnKeyEvent(&event1);
    ui::KeyEvent event2 =
        ui::KeyEvent(ui::EventType::kKeyReleased, code, ui::EF_NONE);
    widget()->OnKeyEvent(&event2);
  }

  void UpdateNotificationViews(const Notification& notification) {
    MessageCenter::Get()->AddNotification(
        std::make_unique<Notification>(notification));
    notification_view_->UpdateWithNotification(notification);
  }

  float GetNotificationSlideAmount() const {
    return notification_view_->GetSlideOutLayer()
        ->transform()
        .To2dTranslation()
        .x();
  }

  bool IsPopupRemovedAfterIdle(const std::string& notification_id) const {
    base::RunLoop().RunUntilIdle();
    return !MessageCenter::Get()->FindPopupNotificationById(notification_id);
  }

  bool IsRemovedAfterIdle(const std::string& notification_id) const {
    base::RunLoop().RunUntilIdle();
    return !MessageCenter::Get()->FindVisibleNotificationById(notification_id);
  }

  void DispatchGesture(const ui::GestureEventDetails& details) {
    ui::GestureEvent event2(0, 0, 0, ui::EventTimeForNow(), details);
    widget()->OnGestureEvent(&event2);
  }

  void BeginScroll() {
    DispatchGesture(
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  }

  void EndScroll() {
    DispatchGesture(ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  }

  void ScrollBy(int dx) {
    DispatchGesture(
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, dx, 0));
  }

  ArcNotificationContentView* content_view() {
    return notification_view_->content_view_;
  }

  views::View* collapsed_summary_view() {
    return notification_view_->collapsed_summary_view_;
  }

  bool IsGroupChild() { return notification_view_->is_group_child_; }

  views::Widget* widget() { return notification_view_->GetWidget(); }
  ArcNotificationView* notification_view() { return notification_view_; }

 protected:
  const std::string kDefaultNotificationKey = "notification_id";
  const std::string kDefaultNotificationId =
      kArcNotificationIdPrefix + kDefaultNotificationKey;

 private:
  std::unique_ptr<message_center::MessageView> CreateCustomMessageViewForTest(
      ArcNotificationItem* item,
      const Notification& notification,
      bool shown_in_popup) {
    auto message_view = std::make_unique<ArcNotificationView>(
        item, notification, shown_in_popup);
    message_view->content_view_->SetPreferredSize(gfx::Size(100, 100));
    return message_view;
  }

  std::unique_ptr<MockArcNotificationSurface> surface_;
  std::unique_ptr<Notification> notification_;
  raw_ptr<ArcNotificationView> notification_view_ =
      nullptr;  // owned by its widget.

  std::unique_ptr<MockArcNotificationItem> item_;
};

TEST_F(ArcNotificationViewTest, Events) {
  widget()->Show();

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(content_view(), &cursor_location);
  EXPECT_EQ(content_view(),
            widget()->GetRootView()->GetEventHandlerForPoint(cursor_location));

  content_view()->RequestFocus();
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(content_view(),
            static_cast<ui::EventTargeter*>(
                widget()->GetRootView()->GetEffectiveViewTargeter())
                ->FindTargetForEvent(widget()->GetRootView(), &key_event));
}

TEST_F(ArcNotificationViewTest, SlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::string notification_id(kDefaultNotificationId);

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-10);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsPopupRemovedAfterIdle(notification_id));
}

// TODO(crbug.com/40889858): Flaky on MSAN bots.
#if defined(MEMORY_SANITIZER)
#define MAYBE_SlideOutNested DISABLED_SlideOutNested
#else
#define MAYBE_SlideOutNested SlideOutNested
#endif
TEST_F(ArcNotificationViewTest, MAYBE_SlideOutNested) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  notification_view()->SetIsNested();
  std::string notification_id(kDefaultNotificationId);

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-10);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsPopupRemovedAfterIdle(notification_id));
}

TEST_F(ArcNotificationViewTest, SlideOutPinned) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_pinned(true);
  notification_view()->SetIsNested();
  UpdateNotificationViews(*notification);
  std::string notification_id(kDefaultNotificationId);

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  EXPECT_LT(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
}

TEST_F(ArcNotificationViewTest, SnoozeButton) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  message_center::RichNotificationData rich_data;
  rich_data.pinned = true;
  rich_data.should_show_snooze_button = true;
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, kDefaultNotificationId,
      u"title", u"message", ui::ImageModel(), u"display source", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "test_app_id"),
      rich_data, nullptr);

  UpdateNotificationViews(*notification);
  notification_view()->SetIsNested();

  EXPECT_NE(nullptr,
            notification_view()->GetControlButtonsView()->snooze_button());
}

TEST_F(ArcNotificationViewTest, PressBackspaceKey) {
  std::string notification_id(kDefaultNotificationId);
  content_view()->RequestFocus();

  ui::InputMethod* input_method = content_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_TRUE(IsRemovedAfterIdle(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(ArcNotificationViewTest, PressBackspaceKeyOnEditBox) {
  std::string notification_id(kDefaultNotificationId);
  content_view()->RequestFocus();

  ui::InputMethod* input_method = content_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_TEXT);

  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(ArcNotificationViewTest, ChangeContentHeight) {
  // Default size.
  gfx::Size size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("100x100", size.ToString());

  // Allow small notifications.
  content_view()->SetPreferredSize(gfx::Size(10, 10));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("10x10", size.ToString());

  // The long notification.
  content_view()->SetPreferredSize(gfx::Size(1000, 1000));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("1000x1000", size.ToString());
}

TEST_F(ArcNotificationViewTest, TrackPadGestureSlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::test::EventGenerator generator(
      (notification_view()->GetWidget()->GetNativeWindow()->GetRootWindow()));
  generator.ScrollSequence(gfx::Point(), base::TimeDelta(), /*x_offset=*/200,
                           /*y_offset=*/0, /*steps=*/1, /*num_fingers=*/2);
  EXPECT_TRUE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
}

class ArcNotificationViewRenderByChromeEnabledTest
    : public ArcNotificationViewTest {
 public:
  ArcNotificationViewRenderByChromeEnabledTest() = default;

  ArcNotificationViewRenderByChromeEnabledTest(
      const ArcNotificationViewRenderByChromeEnabledTest&) = delete;
  ArcNotificationViewRenderByChromeEnabledTest& operator=(
      const ArcNotificationViewRenderByChromeEnabledTest&) = delete;

  ~ArcNotificationViewRenderByChromeEnabledTest() override = default;

  // Overridden from ViewsTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kRenderArcNotificationsByChrome);

    ArcNotificationViewTest::SetUp();
  }

  // Check that smoothness should be recorded after an animation is performed on
  // a particular view.
  // This is copied from
  // ash/system/notification_center/views/ash_notification_view_unittest.cc.
  void CheckSmoothnessRecorded(base::HistogramTester& histograms,
                               views::View* view,
                               const char* animation_histogram_name,
                               int data_point_count = 1) {
    ui::Compositor* compositor = view->layer()->GetCompositor();

    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(view->layer());

    // Force frames and wait for all throughput trackers to be gone to allow
    // animation throughput data to be passed from cc to ui.
    while (compositor->has_throughput_trackers_for_testing()) {
      compositor->ScheduleFullRedraw();
      std::ignore = ui::WaitForNextFrameToBePresented(compositor,
                                                      base::Milliseconds(500));
    }

    // Smoothness should be recorded.
    histograms.ExpectTotalCount(animation_histogram_name, data_point_count);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/324991437)): the test is disabled due to recent flaky results.
TEST_F(ArcNotificationViewRenderByChromeEnabledTest,
       DISABLED_AnimateGroupedChildExpandedCollapseChanged) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->SetGroupChild();
  UpdateNotificationViews(*notification);
  EXPECT_TRUE(IsGroupChild());
  EXPECT_NE(nullptr, collapsed_summary_view());

  // Expected histogram logged when expanding/collapsing.
  notification_view()->AnimateGroupedChildExpandedCollapse(true);

  base::HistogramTester tester_;
  CheckSmoothnessRecorded(
      tester_, collapsed_summary_view(),
      "Arc.NotificationView.CollapsedSummaryView.FadeOut.AnimationSmoothness");

  // Expected behavior in collapsed state.
  notification_view()->AnimateGroupedChildExpandedCollapse(false);

  CheckSmoothnessRecorded(
      tester_, collapsed_summary_view(),
      "Arc.NotificationView.CollapsedSummaryView.FadeIn.AnimationSmoothness");
}

TEST_F(ArcNotificationViewRenderByChromeEnabledTest,
       GroupedChildExpandStateChanged) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->SetGroupChild();
  UpdateNotificationViews(*notification);
  EXPECT_TRUE(IsGroupChild());
  EXPECT_NE(nullptr, collapsed_summary_view());

  // Expected behavior in expanded state.
  notification_view()->SetGroupedChildExpanded(true);
  EXPECT_TRUE(content_view()->GetVisible());
  EXPECT_FALSE(collapsed_summary_view()->GetVisible());

  // Expected behavior in collapsed state.
  notification_view()->SetGroupedChildExpanded(false);
  EXPECT_FALSE(content_view()->GetVisible());
  EXPECT_TRUE(collapsed_summary_view()->GetVisible());
}

}  // namespace ash

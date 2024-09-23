// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/external_arc/message_center/arc_notification_delegate.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_manager.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager_impl.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_item.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/shell.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/key_state.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_modifiers.h"
#include "components/exo/notification_surface.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

constexpr gfx::Rect kNotificationSurfaceBounds(100, 100, 300, 300);

class MockKeyboardDelegate : public exo::KeyboardDelegate {
 public:
  MockKeyboardDelegate() = default;

  // Overridden from KeyboardDelegate:
  MOCK_METHOD(bool,
              CanAcceptKeyboardEventsForSurface,
              (exo::Surface*),
              (const, override));
  MOCK_METHOD(void,
              OnKeyboardEnter,
              (exo::Surface*,
               (const base::flat_map<exo::PhysicalCode,
                                     base::flat_set<exo::KeyState>>&)),
              (override));
  MOCK_METHOD(void, OnKeyboardLeave, (exo::Surface*), (override));
  MOCK_METHOD(uint32_t,
              OnKeyboardKey,
              (base::TimeTicks, ui::DomCode, bool),
              (override));
  MOCK_METHOD(void,
              OnKeyboardModifiers,
              (const exo::KeyboardModifiers&),
              (override));
  MOCK_METHOD(void,
              OnKeyRepeatSettingsChanged,
              (bool, base::TimeDelta, base::TimeDelta),
              (override));
  MOCK_METHOD(void, OnKeyboardLayoutUpdated, (std::string_view), (override));
};

class FakeNotificationSurface : public exo::NotificationSurface {
 public:
  FakeNotificationSurface(exo::NotificationSurfaceManager* manager,
                          exo::Surface* surface,
                          const std::string& notification_key)
      : exo::NotificationSurface(manager, surface, notification_key),
        manager_(manager) {}

  FakeNotificationSurface(const FakeNotificationSurface&) = delete;
  FakeNotificationSurface& operator=(const FakeNotificationSurface&) = delete;

  ~FakeNotificationSurface() override { manager_->RemoveSurface(this); }

 private:
  // Overridden from exo::NotificationSurface:
  void OnSurfaceCommit() override {
    exo::SurfaceTreeHost::OnSurfaceCommit();
    manager_->AddSurface(this);
    // No SubmitCompositorFrame to avoid sync token verification crash due to
    // null SharedMainThreadContextProvider in test under mash.
  }

  const raw_ptr<exo::NotificationSurfaceManager> manager_;  // Not owned.
};

aura::Window* GetFocusedWindow() {
  DCHECK(exo::WMHelper::HasInstance());
  return exo::WMHelper::GetInstance()->GetFocusedWindow();
}

}  // anonymous namespace

class ArcNotificationContentViewTest : public AshTestBase {
 public:
  ArcNotificationContentViewTest() = default;

  ArcNotificationContentViewTest(const ArcNotificationContentViewTest&) =
      delete;
  ArcNotificationContentViewTest& operator=(
      const ArcNotificationContentViewTest&) = delete;

  ~ArcNotificationContentViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    wm_helper_ = std::make_unique<exo::WMHelper>();
    DCHECK(exo::WMHelper::HasInstance());

    surface_manager_ = std::make_unique<ArcNotificationSurfaceManagerImpl>();

    MessageViewFactory::ClearCustomNotificationViewFactory(
        kArcNotificationCustomViewType);
    ArcNotificationManager::SetCustomNotificationViewFactory();
  }

  void TearDown() override {
    // Widget needs to be closed before TearDown() if have been created.
    EXPECT_FALSE(wrapper_widget_);
    EXPECT_FALSE(notification_view_);

    // These may have been initialized in PrepareSurface().
    notification_surface_.reset();
    surface_.reset();
    surface_buffer_.reset();

    surface_manager_.reset();

    DCHECK(exo::WMHelper::HasInstance());
    wm_helper_.reset();

    AshTestBase::TearDown();
  }

  void PressCloseButton(ArcNotificationView* notification_view) {
    ui::test::TestEvent dummy_event;
    auto* control_buttons_view =
        &notification_view->content_view_->control_buttons_view_;
    ASSERT_TRUE(control_buttons_view);
    views::Button* close_button = control_buttons_view->close_button();
    ASSERT_NE(nullptr, close_button);
    close_button->RequestFocus();
    views::test::ButtonTestApi(close_button).NotifyClick(dummy_event);
  }

  void CreateAndShowNotificationView(const Notification& notification) {
    DCHECK(!notification_view_);

    auto result = CreateNotificationView(notification);
    notification_view_ = result.first;
    wrapper_widget_ = std::move(result.second);
    wrapper_widget_->Show();
  }

  std::pair<ArcNotificationView*, std::unique_ptr<views::Widget>>
  CreateNotificationView(const Notification& notification) {
    std::unique_ptr<ArcNotificationView> notification_view(
        static_cast<ArcNotificationView*>(
            MessageViewFactory::Create(notification, /*shown_in_popup=*/false)
                .release()));

    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.context = Shell::GetPrimaryRootWindow();
    auto wrapper_widget = std::make_unique<views::Widget>();
    wrapper_widget->Init(std::move(params));
    ArcNotificationView* notification_view_ptr =
        wrapper_widget->SetContentsView(std::move(notification_view));
    wrapper_widget->SetSize(notification_view_ptr->GetPreferredSize());

    return std::make_pair(notification_view_ptr, std::move(wrapper_widget));
  }

  void CloseNotificationView() {
    wrapper_widget_->Close();
    wrapper_widget_.reset();
    notification_view_ = nullptr;
  }

  void PrepareSurface(const std::string& notification_key) {
    surface_ = std::make_unique<exo::Surface>();
    notification_surface_ = std::make_unique<FakeNotificationSurface>(
        surface_manager(), surface_.get(), notification_key);

    exo::test::ExoTestHelper exo_test_helper;
    surface_buffer_ = exo::test::ExoTestHelper::CreateBuffer(
        kNotificationSurfaceBounds.size());
    surface_->Attach(surface_buffer_.get());

    surface_->Commit();
  }

  Notification CreateNotification(MockArcNotificationItem* notification_item) {
    message_center::RichNotificationData optional_fields;
    optional_fields.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
    Notification notification(
        message_center::NOTIFICATION_TYPE_CUSTOM,
        notification_item->GetNotificationId(), u"title", u"message",
        ui::ImageModel(), u"arc", GURL(),
        message_center::NotifierId(
            message_center::NotifierType::ARC_APPLICATION, "ARC_NOTIFICATION"),
        optional_fields,
        new ArcNotificationDelegate(notification_item->GetWeakPtr()));
    notification.set_custom_view_type(kArcNotificationCustomViewType);
    return notification;
  }

  ArcNotificationSurfaceManagerImpl* surface_manager() {
    return surface_manager_.get();
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }
  exo::Surface* surface() { return surface_.get(); }
  ArcNotificationView* notification_view() { return notification_view_; }

  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const {
    DCHECK(GetArcNotificationContentView());
    return &GetArcNotificationContentView()->control_buttons_view_;
  }
  views::Widget* GetControlButtonsWidget() const {
    DCHECK(GetControlButtonsView()->GetWidget());
    return GetControlButtonsView()->GetWidget();
  }

  ArcNotificationContentView* GetArcNotificationContentView() const {
    return notification_view_->content_view_;
  }

  void ActivateArcNotification() {
    GetArcNotificationContentView()->ActivateWidget(true);
  }

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<ArcNotificationSurfaceManagerImpl> surface_manager_;
  std::unique_ptr<exo::Buffer> surface_buffer_;
  std::unique_ptr<exo::Surface> surface_;
  std::unique_ptr<exo::NotificationSurface> notification_surface_;

  // owned by the |wrapper_widget_|.
  raw_ptr<ArcNotificationView, DanglingUntriaged> notification_view_ = nullptr;
  std::unique_ptr<views::Widget> wrapper_widget_;
};

TEST_F(ArcNotificationContentViewTest, CreateSurfaceAfterNotification) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateSurfaceBeforeNotification) {
  std::string notification_key("notification id");

  PrepareSurface(notification_key);

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateNotificationWithoutSurface) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest,
       CreateSurfaceAfterCollapsingNotification) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  GetArcNotificationContentView()->SetVisible(false);

  PrepareSurface(notification_key);
  EXPECT_FALSE(surface_manager()->GetArcSurface(notification_key)->IsAttached());

  GetArcNotificationContentView()->SetVisible(true);
  EXPECT_TRUE(surface_manager()->GetArcSurface(notification_key)->IsAttached());
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CloseButton) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  PrepareSurface(notification_key);
  Notification notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  // Add a notification with the same ID to the message center so we can verify
  // it gets removed when the close button is pressed. It's SIMPLE instead of
  // ARC to avoid surface shutdown issues.
  auto mc_notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      notification_item->GetNotificationId(), u"title", u"message",
      ui::ImageModel(), u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      message_center::RichNotificationData(), nullptr);
  MessageCenter::Get()->AddNotification(std::move(mc_notification));

  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById(
      notification_item->GetNotificationId()));
  PressCloseButton(notification_view());
  EXPECT_FALSE(MessageCenter::Get()->FindVisibleNotificationById(
      notification_item->GetNotificationId()));

  CloseNotificationView();
}

// Tests pressing close button when hosted in MessageCenterView.
TEST_F(ArcNotificationContentViewTest, CloseButtonInMessageCenterView) {
  std::string notification_key("notification id");

  MessageViewFactory::ClearCustomNotificationViewFactory(
      kArcNotificationCustomViewType);

  // Override MessageView factory to capture the created notification view in
  // |notification_view|.
  ArcNotificationView* notification_view = nullptr;
  MessageViewFactory::SetCustomNotificationViewFactory(
      kArcNotificationCustomViewType,
      base::BindLambdaForTesting(
          [&notification_view](const message_center::Notification& notification,
                               bool shown_in_popup)
              -> std::unique_ptr<message_center::MessageView> {
            auto* arc_delegate =
                static_cast<ArcNotificationDelegate*>(notification.delegate());
            std::unique_ptr<message_center::MessageView> created_view =
                arc_delegate->CreateCustomMessageView(notification,
                                                      /*shown_in_popup=*/false);
            notification_view =
                static_cast<ArcNotificationView*>(created_view.get());
            return created_view;
          }));

  // Show MessageCenterView and activate its widget.
  auto* notification_tray = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                                ->notification_center_tray();
  notification_tray->ShowBubble();

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  PrepareSurface(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  // Sets a close callback so that the underlying item is destroyed when close
  // button is pressed. This simulates ArcNotificationItemImpl behavior.
  notification_item->SetCloseCallback(base::BindLambdaForTesting(
      [&notification_item]() { notification_item.reset(); }));

  MessageCenter::Get()->AddNotification(
      std::make_unique<Notification>(notification));
  ASSERT_TRUE(notification_view);

  // Make sure that the native host can process the located event.
  auto* widget = notification_view->GetWidget();
  auto* root_layer = widget->GetNativeWindow()->layer();
  auto* child_window = notification_view->GetNativeContainerWindowForTest();
  views::internal::NativeWidgetDelegate* native_widget_delegate = widget;
  EXPECT_TRUE(native_widget_delegate->ShouldDescendIntoChildForEventHandling(
      root_layer, child_window, child_window->layer(),
      gfx::Rect(root_layer->bounds().size()).CenterPoint()));

  // Cache notification id because |notification_item| will be gone when the
  // close button is pressed.
  const std::string notification_id = notification_item->GetNotificationId();
  EXPECT_TRUE(
      MessageCenter::Get()->FindVisibleNotificationById(notification_id));
  PressCloseButton(notification_view);
  EXPECT_FALSE(
      MessageCenter::Get()->FindVisibleNotificationById(notification_id));
}

TEST_F(ArcNotificationContentViewTest, CloseButtonPosition) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());
  PrepareSurface(notification_key);

  base::i18n::SetRTLForTesting(false);
  CreateAndShowNotificationView(notification);

  {
    // Focus the close button to make it visible.
    auto* control_buttons_view = GetControlButtonsView();
    ASSERT_TRUE(control_buttons_view);
    views::Button* close_button = control_buttons_view->close_button();
    ASSERT_TRUE(close_button);
    close_button->RequestFocus();

    // In LTR layout, the control buttons should be near top-right.
    auto* notification_content_view = GetArcNotificationContentView();
    ASSERT_TRUE(notification_content_view);
    auto* control_buttons_widget = GetControlButtonsWidget();
    ASSERT_TRUE(control_buttons_widget);
    EXPECT_EQ(
        message_center::kControlButtonPadding *
            2 /* padding for each x and y */,
        control_buttons_widget->GetWindowBoundsInScreen()
            .ManhattanDistanceToPoint(
                notification_content_view->GetBoundsInScreen().top_right()));
  }

  CloseNotificationView();

  // Switch to RTL mode.
  base::i18n::SetRTLForTesting(true);

  CreateAndShowNotificationView(notification);

  {
    // Focus the close button to make it visible.
    auto* control_buttons_view = GetControlButtonsView();
    ASSERT_TRUE(control_buttons_view);
    views::Button* close_button = control_buttons_view->close_button();
    ASSERT_TRUE(close_button);
    close_button->RequestFocus();

    // In RTL layout, The control buttons should be near top-left.
    auto* notification_content_view = GetArcNotificationContentView();
    ASSERT_TRUE(notification_content_view);
    auto* control_buttons_widget = GetControlButtonsWidget();
    ASSERT_TRUE(control_buttons_widget);
    EXPECT_EQ(message_center::kControlButtonPadding *
                  2 /* padding for each x and y */,
              control_buttons_widget->GetWindowBoundsInScreen()
                  .ManhattanDistanceToPoint(
                      notification_content_view->GetBoundsInScreen().origin()));
  }

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ReuseSurfaceAfterClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Use the created surface.
  CreateAndShowNotificationView(notification);
  CloseNotificationView();

  // Reuse.
  CreateAndShowNotificationView(notification);
  CloseNotificationView();

  // Reuse again.
  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ReuseAndCloseSurfaceBeforeClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Create the first view.
  auto wrapper_widget = std::move(CreateNotificationView(notification).second);
  wrapper_widget->Show();

  // Create the second view.
  CreateAndShowNotificationView(notification);
  // Close second view.
  CloseNotificationView();

  // Close the first view.
  wrapper_widget->Close();
  wrapper_widget.reset();
}

TEST_F(ArcNotificationContentViewTest, ReuseSurfaceBeforeClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Create the first view.
  auto wrapper_widget = std::move(CreateNotificationView(notification).second);
  wrapper_widget->Show();

  // Create the second view.
  CreateAndShowNotificationView(notification);

  // Close the first view.
  wrapper_widget->Close();
  wrapper_widget.reset();

  // Close second view.
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, Activate) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(GetFocusedWindow());
  ActivateArcNotification();
  EXPECT_EQ(surface()->window(), GetFocusedWindow());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, NotActivateOnClick) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(GetFocusedWindow());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     kNotificationSurfaceBounds.CenterPoint());
  generator.PressLeftButton();
  EXPECT_EQ(nullptr, GetFocusedWindow());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ActivateWhenRemoteInputOpens) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_EQ(nullptr, GetFocusedWindow());
  GetArcNotificationContentView()->OnRemoteInputActivationChanged(true);
  EXPECT_EQ(surface()->window(), GetFocusedWindow());
  GetArcNotificationContentView()->OnRemoteInputActivationChanged(false);
  EXPECT_NE(surface()->window(), GetFocusedWindow());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, AcceptInputTextWithActivate) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(GetFocusedWindow());
  ActivateArcNotification();
  EXPECT_EQ(surface()->window(), GetFocusedWindow());

  auto delegate = std::make_unique<MockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate, CanAcceptKeyboardEventsForSurface(surface()))
      .WillOnce(testing::Return(true));
  exo::Seat seat;
  auto keyboard = std::make_unique<exo::Keyboard>(std::move(delegate), &seat);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, 0);

  keyboard.reset();

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, NotAcceptInputTextWithoutActivate) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);
  EXPECT_FALSE(GetFocusedWindow());

  auto delegate = std::make_unique<MockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface()))
      .Times(0);
  exo::Seat seat;
  auto keyboard = std::make_unique<exo::Keyboard>(std::move(delegate), &seat);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, testing::_, testing::_))
      .Times(0);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, 0);

  keyboard.reset();

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocus) {
  const bool reverse = false;

  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();

  views::View* view =
      focus_manager->GetNextFocusableView(nullptr, widget(), reverse, true);
  EXPECT_EQ(GetArcNotificationContentView(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetControlButtonsView()->settings_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetControlButtonsView()->close_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(nullptr, view);

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocusReverse) {
  const bool reverse = true;

  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();

  views::View* view =
      focus_manager->GetNextFocusableView(nullptr, widget(), reverse, true);
  EXPECT_EQ(GetControlButtonsView()->close_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetControlButtonsView()->settings_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetArcNotificationContentView(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(nullptr, view);

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocusByTabKey) {
  const std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);
  ActivateArcNotification();

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetArcNotificationContentView(), focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->settings_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->close_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetArcNotificationContentView(), focus_manager->GetFocusedView());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocusReverseByShiftTab) {
  std::string key("notification id");

  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);
  ActivateArcNotification();

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->close_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->settings_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetArcNotificationContentView(), focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->close_button(),
            focus_manager->GetFocusedView());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, AccessibleProperties) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);
  ArcNotificationContentView* content_view = GetArcNotificationContentView();
  ASSERT_TRUE(content_view);
  ArcNotificationSurface* surface = content_view->surface_;
  ui::AXNodeData data;

  ASSERT_TRUE(surface);
  content_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(surface->GetAXTreeId(), ui::AXTreeIDUnknown());
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);

  surface->SetAXTreeId(ui::AXTreeID::CreateNewAXTreeID());
  data = ui::AXNodeData();
  content_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_NE(surface->GetAXTreeId(), ui::AXTreeIDUnknown());
  EXPECT_EQ(data.role, ax::mojom::Role::kClient);

  content_view->SetSurface(nullptr);
  data = ui::AXNodeData();
  content_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);

  auto notification_message = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      notification_item->GetNotificationId(), u"item_title", u"item_message",
      ui::ImageModel(), u"arc", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      message_center::RichNotificationData(), nullptr);

  content_view->Update(*notification_message);
  data = ui::AXNodeData();
  content_view->GetViewAccessibility().GetAccessibleNodeData(&data);

  EXPECT_EQ(u"item_title\nitem_message",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  CloseNotificationView();
}

}  // namespace ash

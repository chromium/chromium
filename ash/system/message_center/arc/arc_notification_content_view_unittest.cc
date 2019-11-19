// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/system/message_center/arc/arc_notification_constants.h"
#include "ash/system/message_center/arc/arc_notification_content_view.h"
#include "ash/system/message_center/arc/arc_notification_delegate.h"
#include "ash/system/message_center/arc/arc_notification_item.h"
#include "ash/system/message_center/arc/arc_notification_manager.h"
#include "ash/system/message_center/arc/arc_notification_surface.h"
#include "ash/system/message_center/arc/arc_notification_surface_manager_impl.h"
#include "ash/system/message_center/arc/arc_notification_view.h"
#include "ash/system/message_center/arc/mock_arc_notification_item.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "components/exo/buffer.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/notification_surface.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/test/views_test_base.h"

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
               (const base::flat_map<ui::DomCode, ui::DomCode>&)),
              (override));
  MOCK_METHOD(void, OnKeyboardLeave, (exo::Surface*), (override));
  MOCK_METHOD(uint32_t,
              OnKeyboardKey,
              (base::TimeTicks, ui::DomCode, bool),
              (override));
  MOCK_METHOD(void, OnKeyboardModifiers, (int), (override));
};

class FakeNotificationSurface : public exo::NotificationSurface {
 public:
  FakeNotificationSurface(exo::NotificationSurfaceManager* manager,
                          exo::Surface* surface,
                          const std::string& notification_key)
      : exo::NotificationSurface(manager, surface, notification_key),
        manager_(manager) {}
  ~FakeNotificationSurface() override { manager_->RemoveSurface(this); }

 private:
  // Overridden from exo::NotificationSurface:
  void OnSurfaceCommit() override {
    exo::SurfaceTreeHost::OnSurfaceCommit();
    manager_->AddSurface(this);
    // No SubmitCompositorFrame to avoid sync token verification crash due to
    // null SharedMainThreadContextProvider in test under mash.
  }

  exo::NotificationSurfaceManager* const manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationSurface);
};

aura::Window* GetFocusedWindow() {
  DCHECK(exo::WMHelper::HasInstance());
  return exo::WMHelper::GetInstance()->GetFocusedWindow();
}

}  // anonymous namespace

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

class ArcNotificationContentViewTest : public AshTestBase {
 public:
  ArcNotificationContentViewTest() = default;
  ~ArcNotificationContentViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    wm_helper_ = std::make_unique<exo::WMHelperChromeOS>();
    exo::WMHelper::SetInstance(wm_helper_.get());
    DCHECK(exo::WMHelper::HasInstance());

    surface_manager_ = std::make_unique<ArcNotificationSurfaceManagerImpl>();

    message_center::MessageViewFactory::
        ClearCustomNotificationViewFactoryForTest(
            kArcNotificationCustomViewType);
    ArcNotificationManager::SetCustomNotificationViewFactory();
  }

  void TearDown() override {
    // Widget and view need to be closed before TearDown() if have been created.
    EXPECT_FALSE(wrapper_widget_);
    EXPECT_FALSE(notification_view_);

    // These may have been initialized in PrepareSurface().
    notification_surface_.reset();
    surface_.reset();
    surface_buffer_.reset();

    surface_manager_.reset();

    DCHECK(exo::WMHelper::HasInstance());
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();

    AshTestBase::TearDown();
  }

  void PressCloseButton(ArcNotificationView* notification_view) {
    DummyEvent dummy_event;
    auto* control_buttons_view =
        &notification_view->content_view_->control_buttons_view_;
    ASSERT_TRUE(control_buttons_view);
    views::Button* close_button = control_buttons_view->close_button();
    ASSERT_NE(nullptr, close_button);
    close_button->RequestFocus();
    control_buttons_view->ButtonPressed(close_button, dummy_event);
  }

  void CreateAndShowNotificationView(const Notification& notification) {
    DCHECK(!notification_view_);

    auto result = CreateNotificationView(notification);
    notification_view_ = std::move(result.first);
    wrapper_widget_ = std::move(result.second);
    wrapper_widget_->Show();
  }

  std::pair<std::unique_ptr<ArcNotificationView>,
            std::unique_ptr<views::Widget>>
  CreateNotificationView(const Notification& notification) {
    std::unique_ptr<ArcNotificationView> notification_view(
        static_cast<ArcNotificationView*>(
            message_center::MessageViewFactory::Create(notification)));
    notification_view->set_owned_by_client();

    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = Shell::GetPrimaryRootWindow();
    auto wrapper_widget = std::make_unique<views::Widget>();
    wrapper_widget->Init(std::move(params));
    wrapper_widget->SetContentsView(notification_view.get());
    wrapper_widget->SetSize(notification_view->GetPreferredSize());

    return std::make_pair(std::move(notification_view),
                          std::move(wrapper_widget));
  }

  void CloseNotificationView() {
    wrapper_widget_->Close();
    wrapper_widget_.reset();

    notification_view_.reset();
  }

  void PrepareSurface(const std::string& notification_key) {
    surface_ = std::make_unique<exo::Surface>();
    notification_surface_ = std::make_unique<FakeNotificationSurface>(
        surface_manager(), surface_.get(), notification_key);

    exo::test::ExoTestHelper exo_test_helper;
    surface_buffer_ =
        std::make_unique<exo::Buffer>(exo_test_helper.CreateGpuMemoryBuffer(
            kNotificationSurfaceBounds.size()));
    surface_->Attach(surface_buffer_.get());

    surface_->Commit();
  }

  Notification CreateNotification(MockArcNotificationItem* notification_item) {
    message_center::RichNotificationData optional_fields;
    optional_fields.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
    Notification notification(
        message_center::NOTIFICATION_TYPE_CUSTOM,
        notification_item->GetNotificationId(), base::UTF8ToUTF16("title"),
        base::UTF8ToUTF16("message"), gfx::Image(), base::UTF8ToUTF16("arc"),
        GURL(),
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
  ArcNotificationView* notification_view() { return notification_view_.get(); }

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
  std::unique_ptr<ArcNotificationView> notification_view_;
  std::unique_ptr<views::Widget> wrapper_widget_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationContentViewTest);
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
      notification_item->GetNotificationId(), base::UTF8ToUTF16("title"),
      base::UTF8ToUTF16("message"), gfx::Image(), base::UTF8ToUTF16("arc"),
      GURL(),
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

  message_center::MessageViewFactory::ClearCustomNotificationViewFactoryForTest(
      kArcNotificationCustomViewType);

  // Override MessageView factory to capture the created notification view in
  // |notification_view|.
  ArcNotificationView* notification_view = nullptr;
  message_center::MessageViewFactory::SetCustomNotificationViewFactory(
      kArcNotificationCustomViewType,
      base::BindLambdaForTesting(
          [&notification_view](const message_center::Notification& notification)
              -> std::unique_ptr<message_center::MessageView> {
            auto* arc_delegate =
                static_cast<ArcNotificationDelegate*>(notification.delegate());
            std::unique_ptr<message_center::MessageView> created_view =
                arc_delegate->CreateCustomMessageView(notification);
            notification_view =
                static_cast<ArcNotificationView*>(created_view.get());
            return created_view;
          }));

  // Show MessageCenterView and activate its widget.
  auto* unified_system_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray();
  unified_system_tray->ShowBubble(false /* show_by_click */);
  unified_system_tray->ActivateBubble();

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

  // Cache notification id because |notification_item| will be gone when the
  // close button is pressed.
  const std::string notification_id = notification_item->GetNotificationId();
  EXPECT_TRUE(
      MessageCenter::Get()->FindVisibleNotificationById(notification_id));
  PressCloseButton(notification_view);
  EXPECT_FALSE(
      MessageCenter::Get()->FindVisibleNotificationById(notification_id));
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
  auto result = CreateNotificationView(notification);
  auto notification_view = std::move(result.first);
  auto wrapper_widget = std::move(result.second);
  wrapper_widget->Show();

  // Create the second view.
  CreateAndShowNotificationView(notification);
  // Close second view.
  CloseNotificationView();

  // Close the first view.
  wrapper_widget->Close();
  wrapper_widget.reset();
  notification_view.reset();
}

TEST_F(ArcNotificationContentViewTest, ReuseSurfaceBeforeClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Create the first view.
  auto result = CreateNotificationView(notification);
  auto notification_view = std::move(result.first);
  auto wrapper_widget = std::move(result.second);
  wrapper_widget->Show();

  // Create the second view.
  CreateAndShowNotificationView(notification);

  // Close the first view.
  wrapper_widget->Close();
  wrapper_widget.reset();
  notification_view.reset();

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

  MockKeyboardDelegate delegate;
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface()))
      .WillOnce(testing::Return(true));
  exo::Seat seat;
  auto keyboard = std::make_unique<exo::Keyboard>(&delegate, &seat);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
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

  MockKeyboardDelegate delegate;
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface())).Times(0);
  exo::Seat seat;
  auto keyboard = std::make_unique<exo::Keyboard>(&delegate, &seat);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, testing::_, testing::_))
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

}  // namespace ash

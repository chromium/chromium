// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include "ash/system/message_center/arc/arc_notification_constants.h"
#include "ash/system/message_center/arc/arc_notification_content_view.h"
#include "ash/system/message_center/arc/arc_notification_surface.h"
#include "ash/system/message_center/arc/arc_notification_surface_manager.h"
#include "ash/system/message_center/arc/arc_notification_view.h"
#include "ash/system/message_center/arc/mock_arc_notification_item.h"
#include "ash/system/message_center/arc/mock_arc_notification_surface.h"
#include "base/command_line.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using ash::ArcNotificationItem;
using ash::ArcNotificationSurface;
using ash::ArcNotificationSurfaceManager;
using ash::ArcNotificationView;
using ash::MockArcNotificationItem;
using ash::MockArcNotificationSurface;

namespace arc {

namespace {

constexpr char kNotificationKey[] = "unit.test.notification";

}  // namespace

class ArcAccessibilityHelperBridgeTest : public ChromeViewsTestBase {
 public:
  class TestArcAccessibilityHelperBridge : public ArcAccessibilityHelperBridge {
   public:
    TestArcAccessibilityHelperBridge(content::BrowserContext* browser_context,
                                     ArcBridgeService* arc_bridge_service)
        : ArcAccessibilityHelperBridge(browser_context, arc_bridge_service),
          window_(new aura::Window(nullptr)),
          event_router_(
              extensions::CreateAndUseTestEventRouter(browser_context)) {
      window_->Init(ui::LAYER_NOT_DRAWN);
    }

    ~TestArcAccessibilityHelperBridge() override { window_.reset(); }

    void SetActiveWindowId(const std::string& id) {
      exo::SetShellApplicationId(window_.get(), id);
    }

    int GetEventCount(const std::string& event_name) const {
      return event_router_->GetEventCount(event_name);
    }

   private:
    aura::Window* GetActiveWindow() override { return window_.get(); }
    extensions::EventRouter* GetEventRouter() const override {
      return event_router_;
    }

    std::unique_ptr<aura::Window> window_;
    extensions::TestEventRouter* const event_router_;

    DISALLOW_COPY_AND_ASSIGN(TestArcAccessibilityHelperBridge);
  };

  class ArcNotificationSurfaceManagerTest
      : public ArcNotificationSurfaceManager {
   public:
    void AddObserver(Observer* observer) override {
      observers_.AddObserver(observer);
    }

    void RemoveObserver(Observer* observer) override {
      observers_.RemoveObserver(observer);
    }

    ArcNotificationSurface* GetArcSurface(
        const std::string& notification_key) const override {
      auto it = surfaces_.find(notification_key);
      if (it == surfaces_.end())
        return nullptr;

      return it->second;
    }

    void AddSurface(ArcNotificationSurface* surface) {
      surfaces_[surface->GetNotificationKey()] = surface;

      for (auto& observer : observers_) {
        observer.OnNotificationSurfaceAdded(surface);
      }
    }

    void RemoveSurface(ArcNotificationSurface* surface) {
      surfaces_.erase(surface->GetNotificationKey());

      for (auto& observer : observers_) {
        observer.OnNotificationSurfaceRemoved(surface);
      }
    }

   private:
    std::map<std::string, ArcNotificationSurface*> surfaces_;
    base::ObserverList<Observer>::Unchecked observers_;
  };

  ArcAccessibilityHelperBridgeTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    testing_profile_ = std::make_unique<TestingProfile>();
    bridge_service_ = std::make_unique<ArcBridgeService>();
    arc_notification_surface_manager_ =
        std::make_unique<ArcNotificationSurfaceManagerTest>();
    accessibility_helper_bridge_ =
        std::make_unique<TestArcAccessibilityHelperBridge>(
            testing_profile_.get(), bridge_service_.get());
  }

  void TearDown() override {
    accessibility_helper_bridge_->Shutdown();
    accessibility_helper_bridge_.reset();
    arc_notification_surface_manager_.reset();
    bridge_service_.reset();
    testing_profile_.reset();

    ChromeViewsTestBase::TearDown();
  }

  TestArcAccessibilityHelperBridge* accessibility_helper_bridge() {
    return accessibility_helper_bridge_.get();
  }

  views::Widget* CreateTestWidget() {
    views::Widget* widget = new views::Widget();
    widget->Init(
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    return widget;
  }

  views::View* GetContentsView(ArcNotificationView* notification_view) {
    return notification_view->content_view_;
  }

  std::unique_ptr<message_center::Notification> CreateNotification() {
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_CUSTOM, kNotificationKey,
        base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"), gfx::Image(),
        base::UTF8ToUTF16("display_source"), GURL(),
        message_center::NotifierId(
            message_center::NotifierType::ARC_APPLICATION, "test_app_id"),
        message_center::RichNotificationData(), nullptr);
    notification->set_custom_view_type(ash::kArcNotificationCustomViewType);
    return notification;
  }

  std::unique_ptr<ArcNotificationView> CreateArcNotificationView(
      ArcNotificationItem* item,
      const message_center::Notification& notification) {
    return std::make_unique<ArcNotificationView>(item, notification);
  }

 protected:
  std::unique_ptr<ArcNotificationSurfaceManagerTest>
      arc_notification_surface_manager_;

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<TestArcAccessibilityHelperBridge>
      accessibility_helper_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridgeTest);
};

TEST_F(ArcAccessibilityHelperBridgeTest, TaskAndAXTreeLifecycle) {
  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  helper_bridge->set_filter_type_all_for_test();

  const auto& task_id_to_tree = helper_bridge->task_id_to_tree_for_test();
  ASSERT_EQ(0U, task_id_to_tree.size());

  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->source_id = 1;
  event1->task_id = 1;
  event1->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[0]->id = 1;
  event1->node_data[0]->string_properties =
      base::flat_map<arc::mojom::AccessibilityStringProperty, std::string>();
  event1->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.android.vending"));
  event1->window_data =
      std::vector<arc::mojom::AccessibilityWindowInfoDataPtr>();
  event1->window_data->push_back(
      arc::mojom::AccessibilityWindowInfoData::New());
  arc::mojom::AccessibilityWindowInfoData* root_window1 =
      event1->window_data->back().get();
  root_window1->window_id = 100;
  root_window1->root_node_id = 1;

  // There's no active window.
  helper_bridge->OnAccessibilityEvent(event1.Clone());
  ASSERT_EQ(0U, task_id_to_tree.size());

  // Let's make task 1 active by activating the window.
  helper_bridge->SetActiveWindowId(std::string("org.chromium.arc.1"));
  helper_bridge->OnAccessibilityEvent(event1.Clone());
  ASSERT_EQ(1U, task_id_to_tree.size());

  // Same package name, different task.
  auto event2 = arc::mojom::AccessibilityEventData::New();
  event2->source_id = 2;
  event2->task_id = 2;
  event2->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event2->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event2->node_data[0]->id = 2;
  event2->node_data[0]->string_properties =
      base::flat_map<arc::mojom::AccessibilityStringProperty, std::string>();
  event2->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.android.vending"));
  event2->window_data =
      std::vector<arc::mojom::AccessibilityWindowInfoDataPtr>();
  event2->window_data->push_back(
      arc::mojom::AccessibilityWindowInfoData::New());
  arc::mojom::AccessibilityWindowInfoData* root_window2 =
      event2->window_data->back().get();
  root_window2->window_id = 200;
  root_window2->root_node_id = 2;

  // Active window is still task 1.
  helper_bridge->OnAccessibilityEvent(event2.Clone());
  ASSERT_EQ(1U, task_id_to_tree.size());

  // Now make task 2 active.
  helper_bridge->SetActiveWindowId(std::string("org.chromium.arc.2"));
  helper_bridge->OnAccessibilityEvent(event2.Clone());
  ASSERT_EQ(2U, task_id_to_tree.size());

  // Same task id, different package name.
  event2->node_data.clear();
  event2->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event2->source_id = 3;
  event2->node_data[0]->id = 3;
  event2->node_data[0]->string_properties =
      base::flat_map<arc::mojom::AccessibilityStringProperty, std::string>();
  event2->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.google.music"));
  root_window2->root_node_id = 3;

  // No new tasks tree mappings should have occurred.
  helper_bridge->OnAccessibilityEvent(event2.Clone());
  ASSERT_EQ(2U, task_id_to_tree.size());

  helper_bridge->OnTaskDestroyed(1);
  ASSERT_EQ(1U, task_id_to_tree.size());

  helper_bridge->OnTaskDestroyed(2);
  ASSERT_EQ(0U, task_id_to_tree.size());
}

TEST_F(ArcAccessibilityHelperBridgeTest, EventAnnouncement) {
  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  helper_bridge->set_filter_type_all_for_test();

  std::vector<std::string> text({"Str"});
  auto event = arc::mojom::AccessibilityEventData::New();
  event->event_type = arc::mojom::AccessibilityEventType::ANNOUNCEMENT;
  event->eventText =
      base::make_optional<std::vector<std::string>>(std::move(text));

  helper_bridge->OnAccessibilityEvent(event.Clone());

  ASSERT_EQ(1, helper_bridge->GetEventCount(
                   extensions::api::accessibility_private::
                       OnAnnounceForAccessibility::kEventName));
}

// Accessibility event and surface creation/removal are sent in different
// channels, mojo and wayland. Order of those events can be changed. This is the
// case where mojo events arrive earlier than surface creation/removal.
//
// mojo: notification 1 created
// wayland: surface 1 added
// mojo: notification 1 removed
// mojo: notification 2 created
// wayland: surface 1 removed
// wayland: surface 2 added
// mojo: notification 2 removed
// wayland: surface 2 removed
TEST_F(ArcAccessibilityHelperBridgeTest, NotificationEventArriveFirst) {
  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  arc_notification_surface_manager_->AddObserver(helper_bridge);

  const auto& notification_key_to_tree_ =
      helper_bridge->notification_key_to_tree_for_test();
  ASSERT_EQ(0U, notification_key_to_tree_.size());

  // mojo: notification 1 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = base::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // wayland: surface 1 added
  MockArcNotificationSurface test_surface(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface);

  // Confirm that axtree id is set to the surface.
  auto it = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it);
  AXTreeSourceArc* tree = it->second.get();
  ui::AXTreeData tree_data;
  tree->GetTreeData(&tree_data);
  EXPECT_EQ(tree_data.tree_id, test_surface.GetAXTreeId());

  // mojo: notification 1 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  // Ax tree of the surface should be reset as the tree no longer exists.
  EXPECT_EQ(ui::AXTreeIDUnknown(), test_surface.GetAXTreeId());

  EXPECT_EQ(0U, notification_key_to_tree_.size());

  // mojo: notification 2 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event3 = arc::mojom::AccessibilityEventData::New();
  event3->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event3->notification_key = base::make_optional<std::string>(kNotificationKey);
  event3->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event3.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // Ax tree from the second event is attached to the first surface. This is
  // expected behavior.
  auto it2 = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it2);
  AXTreeSourceArc* tree2 = it2->second.get();
  ui::AXTreeData tree_data2;
  tree2->GetTreeData(&tree_data2);
  EXPECT_EQ(tree_data2.tree_id, test_surface.GetAXTreeId());

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface);

  // Tree shouldn't be removed as a surface for the second one will come.
  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // wayland: surface 2 added
  MockArcNotificationSurface test_surface_2(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface_2);

  EXPECT_EQ(tree_data2.tree_id, test_surface_2.GetAXTreeId());

  // mojo: notification 2 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  EXPECT_EQ(0U, notification_key_to_tree_.size());

  // wayland: surface 2 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface_2);
}

// This is the case where surface creation/removal arrive before mojo events.
//
// wayland: surface 1 added
// wayland: surface 1 removed
// mojo: notification 1 created
// mojo: notification 1 removed
TEST_F(ArcAccessibilityHelperBridgeTest, NotificationSurfaceArriveFirst) {
  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  arc_notification_surface_manager_->AddObserver(helper_bridge);

  const auto& notification_key_to_tree_ =
      helper_bridge->notification_key_to_tree_for_test();
  ASSERT_EQ(0U, notification_key_to_tree_.size());

  // wayland: surface 1 added
  MockArcNotificationSurface test_surface(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface);

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface);

  // mojo: notification 1 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = base::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // mojo: notification 2 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  EXPECT_EQ(0U, notification_key_to_tree_.size());
}

TEST_F(ArcAccessibilityHelperBridgeTest,
       TextSelectionChangeActivateNotificationWidget) {
  accessibility_helper_bridge()->set_filter_type_all_for_test();

  // Prepare notification surface.
  std::unique_ptr<MockArcNotificationSurface> surface =
      std::make_unique<MockArcNotificationSurface>(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(surface.get());

  // Prepare notification view with ArcNotificationContentView.
  std::unique_ptr<MockArcNotificationItem> item =
      std::make_unique<MockArcNotificationItem>(kNotificationKey);
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification();
  std::unique_ptr<ArcNotificationView> notification_view =
      CreateArcNotificationView(item.get(), *notification.get());
  notification_view->set_owned_by_client();

  // Prepare widget to hold it.
  views::Widget* widget = CreateTestWidget();
  widget->widget_delegate()->SetCanActivate(false);
  widget->Deactivate();
  widget->SetContentsView(notification_view.get());
  widget->Show();

  // Assert that the widget is not activatable.
  ASSERT_FALSE(widget->CanActivate());
  ASSERT_FALSE(widget->IsActive());

  accessibility_helper_bridge()->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);

  // Dispatch text selection changed event.
  auto event = arc::mojom::AccessibilityEventData::New();
  event->event_type =
      arc::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED;
  event->notification_key = base::make_optional<std::string>(kNotificationKey);
  event->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event->window_data =
      std::vector<arc::mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(arc::mojom::AccessibilityWindowInfoData::New());
  accessibility_helper_bridge()->OnAccessibilityEvent(event.Clone());

  // Widget is activated.
  EXPECT_TRUE(widget->CanActivate());
  EXPECT_TRUE(widget->IsActive());

  // Explicitly clear the focus to avoid ArcNotificationContentView::OnBlur is
  // called which fails in this test set up.
  widget->GetFocusManager()->ClearFocus();

  // Widget needs to be closed before the test ends.
  widget->Close();

  // Remove surface cleanly before it's destructed.
  arc_notification_surface_manager_->RemoveSurface(surface.get());
}

TEST_F(ArcAccessibilityHelperBridgeTest, TextSelectionChangedFocusContentView) {
  accessibility_helper_bridge()->set_filter_type_all_for_test();

  // Prepare notification surface.
  std::unique_ptr<MockArcNotificationSurface> surface =
      std::make_unique<MockArcNotificationSurface>(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(surface.get());

  // Prepare notification view with ArcNotificationContentView.
  std::unique_ptr<MockArcNotificationItem> item =
      std::make_unique<MockArcNotificationItem>(kNotificationKey);
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification();
  std::unique_ptr<ArcNotificationView> notification_view =
      CreateArcNotificationView(item.get(), *notification.get());
  notification_view->set_owned_by_client();

  // focus_stealer is a view which has initial focus.
  std::unique_ptr<views::View> focus_stealer = std::make_unique<views::View>();
  focus_stealer->set_owned_by_client();

  // Prepare a widget to hold them.
  views::Widget* widget = CreateTestWidget();
  widget->GetRootView()->AddChildView(notification_view.get());
  widget->GetRootView()->AddChildView(focus_stealer.get());
  widget->Show();

  // Put focus on focus_stealer.
  focus_stealer->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  focus_stealer->RequestFocus();

  // Assert that focus is on focus_stealer.
  ASSERT_TRUE(widget->IsActive());
  ASSERT_EQ(focus_stealer.get(), widget->GetFocusManager()->GetFocusedView());

  accessibility_helper_bridge()->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);

  // Dispatch text selection changed event.
  auto event = arc::mojom::AccessibilityEventData::New();
  event->event_type =
      arc::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED;
  event->notification_key = base::make_optional<std::string>(kNotificationKey);
  event->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event->window_data =
      std::vector<arc::mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(arc::mojom::AccessibilityWindowInfoData::New());
  accessibility_helper_bridge()->OnAccessibilityEvent(event.Clone());

  // Focus moves to contents view with text selection change.
  EXPECT_EQ(GetContentsView(notification_view.get()),
            widget->GetFocusManager()->GetFocusedView());

  // Explicitly clear the focus to avoid ArcNotificationContentView::OnBlur is
  // called which fails in this test set up.
  widget->GetFocusManager()->ClearFocus();

  // Widget needs to be closed before the test ends.
  widget->Close();

  // Remove surface cleanly before it's destructed.
  arc_notification_surface_manager_->RemoveSurface(surface.get());
}

class GetCaptionStyleFromPrefsTests : public ::testing::Test {
 public:
  void SetUp() override {
    prefs_.registry()->RegisterStringPref(prefs::kAccessibilityCaptionsTextSize,
                                          "");
    prefs_.registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsTextColor, "");
    prefs_.registry()->RegisterIntegerPref(
        prefs::kAccessibilityCaptionsTextOpacity, 100);
    prefs_.registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsBackgroundColor, "");
    prefs_.registry()->RegisterIntegerPref(
        prefs::kAccessibilityCaptionsBackgroundOpacity, 100);
    prefs_.registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsTextShadow, "");
    prefs_.registry()->RegisterStringPref(language::prefs::kApplicationLocale,
                                          "");
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(GetCaptionStyleFromPrefsTests, ValidValues) {
  prefs_.SetUserPref(prefs::kAccessibilityCaptionsTextSize,
                     std::make_unique<base::Value>("200%"));
  prefs_.SetUserPref(prefs::kAccessibilityCaptionsTextColor,
                     std::make_unique<base::Value>("10,20,30"));
  prefs_.SetUserPref(prefs::kAccessibilityCaptionsTextOpacity,
                     std::make_unique<base::Value>(90));
  prefs_.SetUserPref(prefs::kAccessibilityCaptionsBackgroundColor,
                     std::make_unique<base::Value>("40,50,60"));
  prefs_.SetUserPref(prefs::kAccessibilityCaptionsBackgroundOpacity,
                     std::make_unique<base::Value>(80));
  prefs_.SetUserPref(
      prefs::kAccessibilityCaptionsTextShadow,
      std::make_unique<base::Value>("-2px -2px 4px rgba(0, 0, 0, 0.5)"));
  prefs_.SetUserPref(language::prefs::kApplicationLocale,
                     std::make_unique<base::Value>("my_locale"));

  auto style = GetCaptionStyleFromPrefs(&prefs_);

  ASSERT_TRUE(style);
  EXPECT_EQ("200%", style->text_size);
  EXPECT_EQ("rgba(10,20,30,0.9)", style->text_color);
  EXPECT_EQ("rgba(40,50,60,0.8)", style->background_color);
  EXPECT_EQ("my_locale", style->user_locale);
  EXPECT_EQ(arc::mojom::CaptionTextShadowType::RAISED, style->text_shadow_type);
}

TEST_F(GetCaptionStyleFromPrefsTests, EmptyValues) {
  auto style = GetCaptionStyleFromPrefs(&prefs_);

  ASSERT_TRUE(style);
  EXPECT_EQ("", style->text_size);
  EXPECT_EQ("", style->text_color);
  EXPECT_EQ("", style->background_color);
  EXPECT_EQ("", style->user_locale);
  EXPECT_EQ(arc::mojom::CaptionTextShadowType::NONE, style->text_shadow_type);
}

}  // namespace arc

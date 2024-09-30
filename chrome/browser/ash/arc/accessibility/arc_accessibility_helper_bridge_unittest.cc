// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_surface.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"

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
  class TestArcAccessibilityHelperBridge
      : public ArcAccessibilityHelperBridge,
        public extensions::TestEventRouter::EventObserver {
   public:
    TestArcAccessibilityHelperBridge(content::BrowserContext* browser_context,
                                     ArcBridgeService* arc_bridge_service)
        : ArcAccessibilityHelperBridge(browser_context, arc_bridge_service),
          event_router_(
              extensions::CreateAndUseTestEventRouter(browser_context)) {
      event_router_->AddEventObserver(this);
    }

    TestArcAccessibilityHelperBridge(const TestArcAccessibilityHelperBridge&) =
        delete;
    TestArcAccessibilityHelperBridge& operator=(
        const TestArcAccessibilityHelperBridge&) = delete;

    ~TestArcAccessibilityHelperBridge() override = default;

    int GetEventCount(const std::string& event_name) const {
      return event_router_->GetEventCount(event_name);
    }

    // TestEventRouter::EventObserver overrides:
    void OnBroadcastEvent(const extensions::Event& event) override {
      last_event = event.DeepCopy();
    }
    void OnDispatchEventToExtension(const std::string& extension_id,
                                    const extensions::Event& event) override {}

    std::unique_ptr<extensions::Event> last_event;

   private:
    // ArcAccessibilityHelperBridge overrides:
    extensions::EventRouter* GetEventRouter() const override {
      return event_router_;
    }
    ax::android::mojom::AccessibilityFilterType GetFilterType() override {
      return filter_type_for_test_;
    }

    const raw_ptr<extensions::TestEventRouter> event_router_;
    ax::android::mojom::AccessibilityFilterType filter_type_for_test_ =
        ax::android::mojom::AccessibilityFilterType::ALL;
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
    std::map<std::string, raw_ptr<ArcNotificationSurface, CtnExperimental>>
        surfaces_;
    base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_;
  };

  ArcAccessibilityHelperBridgeTest() = default;

  ArcAccessibilityHelperBridgeTest(const ArcAccessibilityHelperBridgeTest&) =
      delete;
  ArcAccessibilityHelperBridgeTest& operator=(
      const ArcAccessibilityHelperBridgeTest&) = delete;

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

  views::View* GetContentsView(ArcNotificationView* notification_view) {
    return notification_view->content_view_;
  }

  std::unique_ptr<message_center::Notification> CreateNotification() {
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_CUSTOM, kNotificationKey, u"title",
        u"message", ui::ImageModel(), u"display_source", GURL(),
        message_center::NotifierId(
            message_center::NotifierType::ARC_APPLICATION, "test_app_id"),
        message_center::RichNotificationData(), nullptr);
    notification->set_custom_view_type(ash::kArcNotificationCustomViewType);
    return notification;
  }

  std::unique_ptr<ArcNotificationView> CreateArcNotificationView(
      ArcNotificationItem* item,
      const message_center::Notification& notification) {
    return std::make_unique<ArcNotificationView>(item, notification,
                                                 /*shown_in_popup=*/false);
  }

 protected:
  std::unique_ptr<ArcNotificationSurfaceManagerTest>
      arc_notification_surface_manager_;

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<TestArcAccessibilityHelperBridge>
      accessibility_helper_bridge_;
};

TEST_F(ArcAccessibilityHelperBridgeTest, AnnouncementEvent) {
  const char* const event_name = extensions::api::accessibility_private::
      OnAnnounceForAccessibility::kEventName;

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  const std::string announce_text = "announcement text.";
  std::vector<std::string> text({announce_text});
  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->event_type = ax::android::mojom::AccessibilityEventType::ANNOUNCEMENT;
  event->event_text =
      std::make_optional<std::vector<std::string>>(std::move(text));

  helper_bridge->OnAccessibilityEvent(event.Clone());

  ASSERT_EQ(1, helper_bridge->GetEventCount(event_name));
  ASSERT_EQ(event_name, helper_bridge->last_event->event_name);
  const base::Value::List& arg =
      helper_bridge->last_event->event_args[0].GetList();
  ASSERT_EQ(1U, arg.size());
  ASSERT_EQ(announce_text, arg[0].GetString());
}

TEST_F(ArcAccessibilityHelperBridgeTest, NotificationStateChangedEvent) {
  const char* const event_name = extensions::api::accessibility_private::
      OnAnnounceForAccessibility::kEventName;

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  const std::string toast_text = "announcement text.";
  std::vector<std::string> text({toast_text});
  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->event_type =
      ax::android::mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED;
  event->event_text =
      std::make_optional<std::vector<std::string>>(std::move(text));
  event->string_properties =
      base::flat_map<ax::android::mojom::AccessibilityEventStringProperty,
                     std::string>();
  event->string_properties.value().insert(std::make_pair(
      ax::android::mojom::AccessibilityEventStringProperty::CLASS_NAME,
      "android.widget.Toast$TN"));

  helper_bridge->OnAccessibilityEvent(event.Clone());

  ASSERT_EQ(1, helper_bridge->GetEventCount(event_name));
  ASSERT_EQ(event_name, helper_bridge->last_event->event_name);
  const base::Value::List& arg =
      helper_bridge->last_event->event_args[0].GetList();
  ASSERT_EQ(1U, arg.size());
  ASSERT_EQ(toast_text, arg[0].GetString());

  // Do not announce for non-toast event.
  event->string_properties->clear();
  event->string_properties.value().insert(std::make_pair(
      ax::android::mojom::AccessibilityEventStringProperty::CLASS_NAME,
      "com.android.vending"));

  helper_bridge->OnAccessibilityEvent(event.Clone());

  // Announce event is not dispatched. The event count is not changed.
  ASSERT_EQ(1, helper_bridge->GetEventCount(event_name));
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

  const auto& key_to_tree_ = helper_bridge->trees_for_test();
  ASSERT_EQ(0U, key_to_tree_.size());

  // mojo: notification 1 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event1 = ax::android::mojom::AccessibilityEventData::New();
  event1->event_type =
      ax::android::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = std::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(
      ax::android::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[0]->id = 1;
  event1->window_data =
      std::vector<ax::android::mojom::AccessibilityWindowInfoDataPtr>();
  event1->window_data->push_back(
      ax::android::mojom::AccessibilityWindowInfoData::New());
  event1->window_data->at(0)->window_id = 2;
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, key_to_tree_.size());

  // wayland: surface 1 added
  MockArcNotificationSurface test_surface(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface);

  // Confirm that axtree id is set to the surface.
  ax::android::AXTreeSourceAndroid* tree = key_to_tree_.begin()->second.get();
  ui::AXTreeData tree_data;
  tree->GetTreeData(&tree_data);
  EXPECT_EQ(tree_data.tree_id, test_surface.GetAXTreeId());

  // mojo: notification 1 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  // Ax tree of the surface should be reset as the tree no longer exists.
  EXPECT_EQ(ui::AXTreeIDUnknown(), test_surface.GetAXTreeId());

  EXPECT_EQ(0U, key_to_tree_.size());

  // mojo: notification 2 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event3 = ax::android::mojom::AccessibilityEventData::New();
  event3->event_type =
      ax::android::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event3->notification_key = std::make_optional<std::string>(kNotificationKey);
  event3->node_data.push_back(
      ax::android::mojom::AccessibilityNodeInfoData::New());
  event3->node_data[0]->id = 3;
  event3->window_data =
      std::vector<ax::android::mojom::AccessibilityWindowInfoDataPtr>();
  event3->window_data->push_back(
      ax::android::mojom::AccessibilityWindowInfoData::New());
  event3->window_data->at(0)->window_id = 4;
  helper_bridge->OnAccessibilityEvent(event3.Clone());

  EXPECT_EQ(1U, key_to_tree_.size());

  // Ax tree from the second event is attached to the first surface. This is
  // expected behavior.
  ax::android::AXTreeSourceAndroid* tree2 = key_to_tree_.begin()->second.get();
  ui::AXTreeData tree_data2;
  tree2->GetTreeData(&tree_data2);
  EXPECT_EQ(tree_data2.tree_id, test_surface.GetAXTreeId());

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface);

  // Tree shouldn't be removed as a surface for the second one will come.
  EXPECT_EQ(1U, key_to_tree_.size());

  // wayland: surface 2 added
  MockArcNotificationSurface test_surface_2(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface_2);

  EXPECT_EQ(tree_data2.tree_id, test_surface_2.GetAXTreeId());

  // mojo: notification 2 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  EXPECT_EQ(0U, key_to_tree_.size());

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

  const auto& key_to_tree_ = helper_bridge->trees_for_test();
  ASSERT_EQ(0U, key_to_tree_.size());

  // wayland: surface 1 added
  MockArcNotificationSurface test_surface(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface);

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface);

  // mojo: notification 1 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event1 = ax::android::mojom::AccessibilityEventData::New();
  event1->event_type =
      ax::android::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = std::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(
      ax::android::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[0]->id = 1;
  event1->window_data =
      std::vector<ax::android::mojom::AccessibilityWindowInfoDataPtr>();
  event1->window_data->push_back(
      ax::android::mojom::AccessibilityWindowInfoData::New());
  event1->window_data->at(0)->window_id = 2;
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, key_to_tree_.size());

  // mojo: notification 2 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  EXPECT_EQ(0U, key_to_tree_.size());
}

TEST_F(ArcAccessibilityHelperBridgeTest,
       TextSelectionChangeActivateNotificationWidget) {
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

  // Prepare widget to hold it.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->widget_delegate()->SetCanActivate(false);
  widget->Deactivate();
  widget->SetContentsView(std::move(notification_view));
  widget->Show();

  // Assert that the widget is not activatable.
  ASSERT_FALSE(widget->CanActivate());
  ASSERT_FALSE(widget->IsActive());

  accessibility_helper_bridge()->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);

  // Dispatch text selection changed event.
  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->event_type =
      ax::android::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED;
  event->notification_key = std::make_optional<std::string>(kNotificationKey);
  event->node_data.push_back(
      ax::android::mojom::AccessibilityNodeInfoData::New());
  event->node_data[0]->id = 1;
  event->window_data =
      std::vector<ax::android::mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(
      ax::android::mojom::AccessibilityWindowInfoData::New());
  event->window_data->at(0)->window_id = 2;
  event->source_id = 1;
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
  // Prepare notification surface.
  std::unique_ptr<MockArcNotificationSurface> surface =
      std::make_unique<MockArcNotificationSurface>(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(surface.get());

  // Prepare notification view with ArcNotificationContentView.
  std::unique_ptr<MockArcNotificationItem> item =
      std::make_unique<MockArcNotificationItem>(kNotificationKey);
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification();
  std::unique_ptr<ArcNotificationView> owning_notification_view =
      CreateArcNotificationView(item.get(), *notification.get());

  // focus_stealer is a view which has initial focus.
  std::unique_ptr<views::View> owning_focus_stealer =
      std::make_unique<views::View>();

  // Prepare a widget to hold them.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ArcNotificationView* notification_view =
      widget->GetRootView()->AddChildView(std::move(owning_notification_view));
  views::View* focus_stealer =
      widget->GetRootView()->AddChildView(std::move(owning_focus_stealer));
  widget->Show();

  // Put focus on focus_stealer.
  focus_stealer->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  focus_stealer->RequestFocus();

  // Assert that focus is on focus_stealer.
  ASSERT_TRUE(widget->IsActive());
  ASSERT_EQ(focus_stealer, widget->GetFocusManager()->GetFocusedView());

  accessibility_helper_bridge()->OnNotificationStateChanged(
      kNotificationKey,
      ax::android::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);

  // Dispatch text selection changed event.
  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->event_type =
      ax::android::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED;
  event->notification_key = std::make_optional<std::string>(kNotificationKey);
  event->node_data.push_back(
      ax::android::mojom::AccessibilityNodeInfoData::New());
  event->node_data[0]->id = 1;
  event->window_data =
      std::vector<ax::android::mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(
      ax::android::mojom::AccessibilityWindowInfoData::New());
  event->window_data->at(0)->window_id = 2;
  event->source_id = 1;
  accessibility_helper_bridge()->OnAccessibilityEvent(event.Clone());

  // Focus moves to contents view with text selection change.
  EXPECT_EQ(GetContentsView(notification_view),
            widget->GetFocusManager()->GetFocusedView());

  // Explicitly clear the focus to avoid ArcNotificationContentView::OnBlur is
  // called which fails in this test set up.
  widget->GetFocusManager()->ClearFocus();

  // Widget needs to be closed before the test ends.
  widget->Close();

  // Remove surface cleanly before it's destructed.
  arc_notification_surface_manager_->RemoveSurface(surface.get());
}

}  // namespace arc

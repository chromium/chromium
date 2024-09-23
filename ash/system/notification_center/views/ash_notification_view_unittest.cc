// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/ash_notification_view.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/draggable_test_view.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/notification_center/ash_notification_drag_controller.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_style.h"
#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/ash_notification_expand_button.h"
#include "ash/system/notification_center/views/ash_notification_input_container.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/system/notification_center/views/timestamp_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "ui/base/data_transfer_policy/mock_data_transfer_policy_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using message_center::Notification;
using message_center::NotificationHeaderView;
using message_center::NotificationView;
using ::testing::_;
using ::testing::Bool;
using ::testing::UnorderedElementsAre;

// Constants -------------------------------------------------------------------

// The time duration that ensures notifications become aging enough for removal.
constexpr base::TimeDelta kNotificationAgingWaitTime = base::Seconds(2);

// The notification count limit in tests.
constexpr size_t kOverridingCountLimit = 5;

// Target notification count after cleaning in tests.
constexpr size_t kOverridingTargetCountAfterRemoval = 3;

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";

// A radomly selected time duration for waiting.
constexpr base::TimeDelta kWaitTime = base::Milliseconds(500);

// Matchers --------------------------------------------------------------------

MATCHER_P(NotificationIdMatches, target_id, "") {
  return arg->id() == target_id;
}

// Helper classes --------------------------------------------------------------

class NotificationTestDelegate : public message_center::NotificationDelegate {
 public:
  NotificationTestDelegate() = default;
  NotificationTestDelegate(const NotificationTestDelegate&) = delete;
  NotificationTestDelegate& operator=(const NotificationTestDelegate&) = delete;

  void DisableNotification() override { disable_notification_called_ = true; }

  bool disable_notification_called() const {
    return disable_notification_called_;
  }

 private:
  ~NotificationTestDelegate() override = default;

  bool disable_notification_called_ = false;
};

// A helper class to wait for the target message center visibility.
class MessageCenterTargetVisibilityWaiter
    : public message_center::MessageCenterObserver {
 public:
  explicit MessageCenterTargetVisibilityWaiter(bool target_visible)
      : target_visible_(target_visible) {
    observation_.Observe(message_center::MessageCenter::Get());
  }

  void Wait() {
    if (message_center::MessageCenter::Get()->IsMessageCenterVisible() !=
        target_visible_) {
      run_loop_.Run();
    }
  }

 private:
  // message_center::MessageCenterObserver:
  void OnCenterVisibilityChanged(
      message_center::Visibility visibility) override {
    if (run_loop_.running()) {
      const bool is_actually_visible =
          (visibility == message_center::VISIBILITY_MESSAGE_CENTER);
      if (target_visible_ == is_actually_visible) {
        run_loop_.Quit();
      }
    }
  }

  // The target message center visibility.
  const bool target_visible_;

  base::RunLoop run_loop_;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      observation_{this};
};

// A mocked delegate to verify the notification drag-and-drop.
class MockAshNotificationDragDropDelegate
    : public aura::client::DragDropDelegate {
 public:
  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}

  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override {
    return aura::client::DragUpdateInfo(
        ui::DragDropTypes::DRAG_COPY,
        ui::DataTransferEndpoint(ui::EndpointType::kDefault));
  }

  void OnDragExited() override {}

  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override {
    return base::BindOnce(&MockAshNotificationDragDropDelegate::PerformDrop,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // A mocked function to handle the html data in the drag-and-drop data.
  MOCK_METHOD(void, HandleHtmlData, (), (const));

  // A mocked function to handle the file path data in the drag-and-drop data.
  MOCK_METHOD(void, HandleFilePathData, (const base::FilePath&), (const));

 private:
  void PerformDrop(std::unique_ptr<ui::OSExchangeData> data,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    if (data->HasHtml() || data->HasFile()) {
      output_drag_op = ui::mojom::DragOperation::kCopy;
      if (data->HasHtml()) {
        HandleHtmlData();
      } else {
        std::optional<std::vector<ui::FileInfo>> files = data->GetFilenames();
        HandleFilePathData(files.value()[0].path);
      }
    }
  }

  base::WeakPtrFactory<MockAshNotificationDragDropDelegate> weak_ptr_factory_{
      this};
};

}  // namespace

// A test base class that helps to verify notification view features.
class AshNotificationViewTestBase : public AshTestBase,
                                    public views::ViewObserver {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit AshNotificationViewTestBase(TaskEnvironmentTraits&&... traits)
      : AshTestBase(traits...) {}
  AshNotificationViewTestBase(const AshNotificationViewTestBase&) = delete;
  AshNotificationViewTestBase& operator=(const AshNotificationViewTestBase&) =
      delete;
  ~AshNotificationViewTestBase() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = new NotificationTestDelegate();
    notification_center_test_api_ =
        std::make_unique<NotificationCenterTestApi>();
  }

  // Create a test notification that is used in the view.
  std::unique_ptr<Notification> CreateTestNotification(
      bool has_image = false,
      bool show_snooze_button = false,
      bool has_message = true,
      message_center::NotificationType notification_type =
          message_center::NOTIFICATION_TYPE_SIMPLE,
      const std::optional<base::FilePath>& image_path = std::nullopt,
      bool pinned = false,
      message_center::NotificationPriority priority =
          message_center::NotificationPriority::DEFAULT_PRIORITY) {
    message_center::RichNotificationData data;
    data.settings_button_handler =
        message_center::SettingsButtonHandler::INLINE;
    data.should_show_snooze_button = show_snooze_button;
    if (image_path) {
      data.image_path = *image_path;
    }
    data.pinned = pinned;
    data.priority = priority;

    std::u16string message = has_message ? u"message" : u"";

    std::unique_ptr<Notification> notification = std::make_unique<Notification>(
        notification_type, base::NumberToString(current_id_++), u"title",
        message, ui::ImageModel::FromImage(gfx::test::CreateImage(/*size=*/80)),
        u"display source", GURL(),
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   "extension_id"),
        data, delegate_);
    notification->SetSmallImage(gfx::test::CreateImage(/*size=*/16));

    if (has_image) {
      notification->SetImage(gfx::test::CreateImage(320, 240));
    }

    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(*notification));

    return notification;
  }

  // Create a test notification. All the notifications created by this function
  // will belong to the same group.
  std::unique_ptr<Notification> CreateTestNotificationInAGroup(
      bool has_image = false,
      const std::optional<base::FilePath>& image_path = std::nullopt,
      message_center::NotificationPriority priority =
          message_center::DEFAULT_PRIORITY) {
    message_center::NotifierId notifier_id;
    notifier_id.profile_id = "abc@gmail.com";
    notifier_id.type = message_center::NotifierType::WEB_PAGE;

    message_center::RichNotificationData rich_notification_data;
    if (image_path) {
      rich_notification_data.image_path = *image_path;
    }
    rich_notification_data.priority = priority;

    std::unique_ptr<Notification> notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        base::NumberToString(current_id_++), u"title", u"message",
        ui::ImageModel::FromImage(gfx::test::CreateImage(/*size=*/80)),
        u"display source", GURL(u"http://test-url.com"), notifier_id,
        rich_notification_data, delegate_);
    notification->SetSmallImage(gfx::test::CreateImage(/*size=*/16));

    if (has_image) {
      notification->SetImage(gfx::test::CreateImage(320, 240));
    }

    message_center::MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(*notification));

    return notification;
  }

  // Get the tested notification view from message center. This is used in
  // checking smoothness metrics: The check requires the use of the compositor,
  // which we don't have in the customed made `notification_view_`.
  AshNotificationView* GetNotificationViewFromMessageCenter(std::string id) {
    return static_cast<AshNotificationView*>(
        notification_center_test_api_->GetNotificationViewForId(id));
  }

  void UpdateTimestampForNotification(AshNotificationView* notification_view,
                                      base::Time timestamp) {
    notification_view->title_row_->UpdateTimestamp(timestamp);
  }

  // Toggle inline settings with a dummy event.
  void ToggleInlineSettings(AshNotificationView* view) {
    view->ToggleInlineSettings(ui::test::TestEvent());
  }

  // Make the given notification to become a group parent of some basic
  // notifications.
  void MakeNotificationGroupParent(AshNotificationView* view,
                                   int group_child_num) {
    auto* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            view->notification_id());
    notification->SetGroupParent();
    view->UpdateWithNotification(*notification);
    for (int i = 0; i < 1; i++) {
      auto group_child = CreateTestNotification();
      group_child->SetGroupChild();
      view->AddGroupNotification(*group_child.get());
    }
  }

  // Check that smoothness should be recorded after an animation is performed on
  // a particular view.
  void CheckSmoothnessRecorded(base::HistogramTester& histograms,
                               views::View* view,
                               const std::string& animation_histogram_name,
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

 protected:
  AshNotificationView* GetFirstGroupedChildNotificationView(
      AshNotificationView* view) {
    if (!view->grouped_notifications_container_->children().size()) {
      return nullptr;
    }
    return static_cast<AshNotificationView*>(
        view->grouped_notifications_container_->children().front());
  }
  std::vector<raw_ptr<views::View, VectorExperimental>> GetChildNotifications(
      AshNotificationView* view) {
    return view->grouped_notifications_container_->children();
  }
  views::View* GetMainView(AshNotificationView* view) {
    return view->main_view_;
  }
  views::View* GetMainRightView(AshNotificationView* view) {
    return view->main_right_view_;
  }
  NotificationHeaderView* GetHeaderRow(AshNotificationView* view) {
    return view->header_row();
  }
  views::View* GetLeftContent(AshNotificationView* view) {
    return view->left_content();
  }
  views::View* GetTitleRowDivider(AshNotificationView* view) {
    return view->title_row_->title_row_divider_;
  }
  views::Label* GetTimestampInCollapsedView(AshNotificationView* view) {
    return view->title_row_->timestamp_in_collapsed_view_;
  }
  const views::Label* GetTimestamp(AshNotificationView* view) {
    return view->header_row()->timestamp_view_for_testing();
  }

  views::Label* GetMessageLabel(AshNotificationView* view) {
    return view->message_label();
  }
  views::Label* GetMessageLabelInExpandedState(AshNotificationView* view) {
    return view->message_label_in_expanded_state_;
  }
  message_center::ProportionalImageView* GetIconView(
      AshNotificationView* view) const {
    return view->icon_view();
  }
  AshNotificationExpandButton* GetExpandButton(AshNotificationView* view) {
    return view->expand_button_;
  }
  views::View* GetCollapsedSummaryView(AshNotificationView* view) {
    return view->collapsed_summary_view_;
  }
  views::View* GetImageContainerView(AshNotificationView* view) {
    return view->image_container_view();
  }
  views::View* GetActionsRow(AshNotificationView* view) {
    return view->actions_row();
  }
  views::View* GetActionButtonsRow(AshNotificationView* view) {
    return view->action_buttons_row();
  }
  std::vector<raw_ptr<views::LabelButton, VectorExperimental>> GetActionButtons(
      AshNotificationView* view) {
    return view->action_buttons();
  }
  message_center::NotificationInputContainer* GetInlineReply(
      AshNotificationView* view) {
    return view->inline_reply();
  }
  views::View* GetInlineSettingsRow(AshNotificationView* view) {
    return view->inline_settings_row();
  }
  views::View* GetGroupedNotificationsContainer(AshNotificationView* view) {
    return view->grouped_notifications_container_;
  }
  views::View* GetContentRow(AshNotificationView* view) {
    return view->content_row();
  }
  RoundedImageView* GetAppIconView(AshNotificationView* view) {
    return view->app_icon_view_;
  }
  views::View* GetTitleRow(AshNotificationView* view) {
    return view->title_row_;
  }
  views::Label* GetTitleView(AshNotificationView* view) {
    return view->title_row_->title_view_;
  }
  views::Button* GetTurnOffNotificationsButton(AshNotificationView* view) {
    return static_cast<views::Button*>(
        view->GetViewByID(kNotificationTurnOffNotificationsButton));
  }
  views::Button* GetInlineSettingsCancelButton(AshNotificationView* view) {
    return static_cast<views::Button*>(
        view->GetViewByID(kNotificationInlineSettingsCancelButton));
  }
  IconButton* GetSnoozeButton(AshNotificationView* view) {
    return view->snooze_button_;
  }

  scoped_refptr<NotificationTestDelegate> delegate() { return delegate_; }

  NotificationCenterTestApi* notification_center_test_api() {
    return notification_center_test_api_.get();
  }

 private:
  scoped_refptr<NotificationTestDelegate> delegate_;
  std::unique_ptr<NotificationCenterTestApi> notification_center_test_api_;

  // Used to create test notification. This represents the current available
  // number that we can use to create the next test notification. This id will
  // be incremented whenever we create a new test notification.
  int current_id_ = 0;
};

// The notification view test class that uses the mock time.
class AshNotificationViewTest : public AshNotificationViewTestBase {
 public:
  AshNotificationViewTest()
      : AshNotificationViewTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // AshNotificationViewTestBase:
  void SetUp() override {
    AshNotificationViewTestBase::SetUp();
    auto notification = CreateTestNotification();
    auto notification_view = std::make_unique<AshNotificationView>(
        *notification, /*is_popup=*/false);
    notification_view_ = notification_view.get();
    test_widget_ = CreateFramelessTestWidget();
    test_widget_->SetContentsView(std::move(notification_view));
    test_widget_->SetSize({400, 100});
    test_widget_->Show();
  }

  void TearDown() override {
    // Drop the pointer before it's freed by the Widget.
    notification_view_ = nullptr;
    test_widget_.reset();
    AshTestBase::TearDown();
  }

  AshNotificationView* notification_view() { return notification_view_.get(); }

  views::Widget* test_widget() { return test_widget_.get(); }

 private:
  raw_ptr<AshNotificationView> notification_view_;
  std::unique_ptr<views::Widget> test_widget_;
};

TEST_F(AshNotificationViewTest, UpdateViewsOrderingTest) {
  EXPECT_NE(nullptr, GetTitleRow(notification_view()));
  EXPECT_NE(nullptr, GetMessageLabel(notification_view()));
  EXPECT_EQ(0u, GetLeftContent(notification_view())
                    ->GetIndexOf(GetTitleRow(notification_view())));
  EXPECT_EQ(1u, GetLeftContent(notification_view())
                    ->GetIndexOf(GetMessageLabel(notification_view())));

  std::unique_ptr<Notification> notification = CreateTestNotification();
  notification->set_title(std::u16string());

  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(nullptr, GetTitleRow(notification_view()));
  EXPECT_NE(nullptr, GetMessageLabel(notification_view()));
  EXPECT_EQ(0u, GetLeftContent(notification_view())
                    ->GetIndexOf(GetMessageLabel(notification_view())));

  notification->set_title(u"title");

  notification_view()->UpdateWithNotification(*notification);

  EXPECT_NE(nullptr, GetTitleRow(notification_view()));
  EXPECT_NE(nullptr, GetMessageLabel(notification_view()));
  EXPECT_EQ(0u, GetLeftContent(notification_view())
                    ->GetIndexOf(GetTitleRow(notification_view())));
  EXPECT_EQ(1u, GetLeftContent(notification_view())
                    ->GetIndexOf(GetMessageLabel(notification_view())));
}

TEST_F(AshNotificationViewTest, CreateOrUpdateTitle) {
  EXPECT_NE(nullptr, GetTitleRow(notification_view()));
  EXPECT_NE(nullptr, GetTitleView(notification_view()));
  EXPECT_NE(nullptr, GetTitleRowDivider(notification_view()));
  EXPECT_NE(nullptr, GetTimestampInCollapsedView(notification_view()));

  std::unique_ptr<Notification> notification = CreateTestNotification();

  // Every view should be null when title is empty.
  notification->set_title(std::u16string());
  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(nullptr, GetTitleRow(notification_view()));

  const std::u16string& expected_text = u"title";
  notification->set_title(expected_text);

  notification_view()->UpdateWithNotification(*notification);

  EXPECT_NE(nullptr, GetTitleRow(notification_view()));
  EXPECT_EQ(expected_text, GetTitleView(notification_view())->GetText());
}

TEST_F(AshNotificationViewTest, ExpandCollapseBehavior) {
  auto notification = CreateTestNotification(/*has_image=*/true);
  notification_view()->UpdateWithNotification(*notification);

  // Expected behavior in collapsed mode.
  notification_view()->SetExpanded(false);
  EXPECT_FALSE(GetHeaderRow(notification_view())->GetVisible());
  EXPECT_TRUE(GetTimestampInCollapsedView(notification_view())->GetVisible());
  EXPECT_TRUE(GetTitleRowDivider(notification_view())->GetVisible());
  EXPECT_TRUE(GetMessageLabel(notification_view())->GetVisible());
  EXPECT_FALSE(
      GetMessageLabelInExpandedState(notification_view())->GetVisible());

  // Expected behavior in expanded mode.
  notification_view()->SetExpanded(true);
  EXPECT_TRUE(GetHeaderRow(notification_view())->GetVisible());
  EXPECT_FALSE(GetTimestampInCollapsedView(notification_view())->GetVisible());
  EXPECT_FALSE(GetTitleRowDivider(notification_view())->GetVisible());
  EXPECT_FALSE(GetMessageLabel(notification_view())->GetVisible());
  EXPECT_TRUE(
      GetMessageLabelInExpandedState(notification_view())->GetVisible());
}

TEST_F(AshNotificationViewTest, ManuallyExpandedOrCollapsed) {
  // Test |manually_expanded_or_collapsed| being set when the toggle is done by
  // user interaction.
  EXPECT_FALSE(notification_view()->IsManuallyExpandedOrCollapsed());
  notification_view()->ToggleExpand();
  EXPECT_TRUE(notification_view()->IsManuallyExpandedOrCollapsed());
}

TEST_F(AshNotificationViewTest, GroupedNotificationStartsCollapsed) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);
  MakeNotificationGroupParent(
      notification_view(),
      message_center_style::kMaxGroupedNotificationsInCollapsedState);

  // Grouped notification should start collapsed.
  EXPECT_FALSE(notification_view()->IsExpanded());
  EXPECT_TRUE(GetHeaderRow(notification_view())->GetVisible());
  EXPECT_TRUE(GetExpandButton(notification_view())->label()->GetVisible());
}

TEST_F(AshNotificationViewTest, GroupedNotificationCounterVisibility) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);
  MakeNotificationGroupParent(
      notification_view(),
      message_center_style::kMaxGroupedNotificationsInCollapsedState + 1);

  EXPECT_TRUE(GetExpandButton(notification_view())->label()->GetVisible());

  auto* child_view = GetFirstGroupedChildNotificationView(notification_view());
  EXPECT_TRUE(GetCollapsedSummaryView(child_view)->GetVisible());
  EXPECT_FALSE(GetMainView(child_view)->GetVisible());
}

TEST_F(AshNotificationViewTest, GroupedNotificationExpandState) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);
  MakeNotificationGroupParent(
      notification_view(),
      message_center_style::kMaxGroupedNotificationsInCollapsedState + 1);

  auto* child_view = GetFirstGroupedChildNotificationView(notification_view());
  EXPECT_TRUE(GetCollapsedSummaryView(child_view)->GetVisible());
  EXPECT_FALSE(GetMainView(child_view)->GetVisible());
  EXPECT_TRUE(GetTimestamp(notification_view())->GetVisible());
  // Expanding the parent notification should make the expand button counter and
  // timestamp invisible and the child notifications should now have the main
  // view visible instead of the summary.
  notification_view()->SetExpanded(true);
  EXPECT_FALSE(GetExpandButton(notification_view())->label()->GetVisible());
  EXPECT_FALSE(GetTimestamp(notification_view())->GetVisible());
  EXPECT_FALSE(GetCollapsedSummaryView(child_view)->GetVisible());
  EXPECT_TRUE(GetMainView(child_view)->GetVisible());
}

TEST_F(AshNotificationViewTest, GroupedNotificationChildIcon) {
  auto notification = CreateTestNotification();
  notification->set_icon(
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16, SK_ColorBLUE)));
  notification->SetGroupChild();
  notification_view()->UpdateWithNotification(*notification.get());

  // Notification's icon should be used in child notification's app icon (we
  // check this by comparing the color of the app icon with the color of the
  // generated test image).
  EXPECT_EQ(color_utils::SkColorToRgbaString(SK_ColorBLUE),
            color_utils::SkColorToRgbaString(GetAppIconView(notification_view())
                                                 ->original_image()
                                                 .bitmap()
                                                 ->getColor(0, 0)));

  // This should not be changed after theme changed.
  notification_view()->OnThemeChanged();
  EXPECT_EQ(color_utils::SkColorToRgbaString(SK_ColorBLUE),
            color_utils::SkColorToRgbaString(GetAppIconView(notification_view())
                                                 ->original_image()
                                                 .bitmap()
                                                 ->getColor(0, 0)));

  // Reset the notification to be group parent at the end.
  notification->SetGroupParent();
  notification_view()->UpdateWithNotification(*notification.get());
}

TEST_F(AshNotificationViewTest,
       GroupedNotificationExpandCollapseStateVisibility) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);
  MakeNotificationGroupParent(
      notification_view(),
      4 * message_center_style::kMaxGroupedNotificationsInCollapsedState);

  // Only the first `kMaxGroupedNotificationsInCollapsedState` grouped
  // notifications should be visible in the collapsed state.
  int counter = 0;
  for (views::View* child : GetChildNotifications(notification_view())) {
    if (counter <
        message_center_style::kMaxGroupedNotificationsInCollapsedState) {
      EXPECT_TRUE(child->GetVisible());
    } else {
      EXPECT_FALSE(child->GetVisible());
    }
    counter++;
  }

  // All grouped notifications should be visible once the parent is expanded.
  notification_view()->SetExpanded(true);
  for (views::View* child : GetChildNotifications(notification_view())) {
    EXPECT_TRUE(child->GetVisible());
  }

  notification_view()->SetExpanded(false);

  // Going back to collapsed state only the first
  // `kMaxGroupedNotificationsInCollapsedState` grouped notifications should be
  // visible.
  counter = 0;
  for (views::View* child : GetChildNotifications(notification_view())) {
    if (counter <
        message_center_style::kMaxGroupedNotificationsInCollapsedState) {
      EXPECT_TRUE(child->GetVisible());
    } else {
      EXPECT_FALSE(child->GetVisible());
    }
    counter++;
  }
}

TEST_F(AshNotificationViewTest, ExpandButtonVisibility) {
  // Expand button should be shown in any type of notification and hidden in
  // inline settings UI.
  auto notification1 = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification1);
  EXPECT_TRUE(GetExpandButton(notification_view())->GetVisible());

  auto notification2 = CreateTestNotification(/*has_image=*/true);
  notification_view()->UpdateWithNotification(*notification2);
  EXPECT_TRUE(GetExpandButton(notification_view())->GetVisible());

  ToggleInlineSettings(notification_view());
  // `content_row()` should be hidden, which also means expand button should be
  // hidden here.
  EXPECT_FALSE(GetExpandButton(notification_view())->GetVisible());

  // Toggle back.
  ToggleInlineSettings(notification_view());
  EXPECT_TRUE(GetContentRow(notification_view())->GetVisible());
  EXPECT_TRUE(GetExpandButton(notification_view())->GetVisible());
}

TEST_F(AshNotificationViewTest, WarningLevelInSummaryText) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);

  // Notification with normal system warning level should have empty summary
  // text.
  EXPECT_EQ(
      std::u16string(),
      GetHeaderRow(notification_view())->summary_text_for_testing()->GetText());

  // Notification with warning/critical warning level should display a text in
  // summary text.
  notification->set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::WARNING);
  notification_view()->UpdateWithNotification(*notification);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_NOTIFICATION_WARNING_LABEL),
      GetHeaderRow(notification_view())->summary_text_for_testing()->GetText());

  notification->set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification_view()->UpdateWithNotification(*notification);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_NOTIFICATION_CRITICAL_WARNING_LABEL),
      GetHeaderRow(notification_view())->summary_text_for_testing()->GetText());
}

TEST_F(AshNotificationViewTest, InlineSettingsBlockAll) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);

  ToggleInlineSettings(notification_view());
  EXPECT_TRUE(GetInlineSettingsRow(notification_view())->GetVisible());

  // Clicking the turn off button should disable notifications.
  views::test::ButtonTestApi test_api(
      GetTurnOffNotificationsButton(notification_view()));
  test_api.NotifyClick(ui::test::TestEvent());
  EXPECT_TRUE(delegate()->disable_notification_called());
}

TEST_F(AshNotificationViewTest, InlineSettingsCancel) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);

  ToggleInlineSettings(notification_view());
  EXPECT_TRUE(GetInlineSettingsRow(notification_view())->GetVisible());

  // Clicking the cancel button should not disable notifications.
  views::test::ButtonTestApi test_api(
      GetInlineSettingsCancelButton(notification_view()));
  test_api.NotifyClick(ui::test::TestEvent());

  EXPECT_FALSE(GetInlineSettingsRow(notification_view())->GetVisible());
  EXPECT_FALSE(delegate()->disable_notification_called());
}

TEST_F(AshNotificationViewTest, SnoozeButtonVisibility) {
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);

  // Snooze button should be null if notification does not use it.
  EXPECT_EQ(GetSnoozeButton(notification_view()), nullptr);

  notification =
      CreateTestNotification(/*has_image=*/false, /*show_snooze_button=*/true);
  notification_view()->UpdateWithNotification(*notification);

  // Snooze button should be visible if notification does use it.
  EXPECT_TRUE(GetSnoozeButton(notification_view())->GetVisible());
}

// Test to ensure the snooze button is correctly displayed after a notification
// update.
TEST_F(AshNotificationViewTest, SnoozeButtonVisibilityAfterNotificationUpdate) {
  auto notification =
      CreateTestNotification(/*has_image=*/false, /*show_snooze_button=*/true);
  notification_view()->UpdateWithNotification(*notification);

  // Make sure the notification is expanded.
  notification_view()->ToggleExpand();
  ASSERT_TRUE(notification_view()->IsExpanded());

  // Simulate an update to a notification which is already displayed.
  notification_view()->UpdateWithNotification(*notification);
  // The actions row which contains the snooze button should be visible.
  EXPECT_TRUE(GetActionsRow(notification_view())->GetVisible());
}

TEST_F(AshNotificationViewTest, AppIconAndExpandButtonAlignment) {
  auto notification = CreateTestNotification(/*has_image=*/true);
  notification_view()->UpdateWithNotification(*notification);
  ASSERT_GT(notification_view()->width(), 0);
  ASSERT_GT(notification_view()->height(), 0);

  // Make sure that app icon and expand button is vertically aligned in
  // collapsed mode.
  notification_view()->SetExpanded(false);
  EXPECT_EQ(GetAppIconView(notification_view())->GetBoundsInScreen().y(),
            GetExpandButton(notification_view())->GetBoundsInScreen().y());

  // Make sure that app icon, expand button, and header row are top-aligned
  // (have the same y anchor) in expanded mode.
  notification_view()->SetExpanded(true);

  // Need to run layout after expand or the header is not sized correctly.
  views::test::RunScheduledLayout(test_widget());

  ASSERT_GT(GetHeaderRow(notification_view())->bounds().height(), 0);
  ASSERT_TRUE(GetHeaderRow(notification_view())->GetVisible());

  EXPECT_EQ(GetAppIconView(notification_view())->GetBoundsInScreen().y(),
            GetExpandButton(notification_view())->GetBoundsInScreen().y());
  EXPECT_EQ(GetAppIconView(notification_view())->bounds().y(),
            GetHeaderRow(notification_view())->bounds().y());
}

TEST_F(AshNotificationViewTest, ExpandCollapseAnimationsRecordSmoothness) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);
  auto notification =
      CreateTestNotification(/*has_image=*/true, /*show_snooze_button=*/true);
  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());

  // Use long message to show `message_label_in_expanded_state_`.
  notification->set_message(
      u"consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
      u"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      u"exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      u"consequat.");
  message_center::MessageCenter::Get()->UpdateNotification(
      notification->id(), std::move(notification));

  EXPECT_TRUE(notification_view->IsExpanded());

  base::HistogramTester histograms_collapsed;
  notification_view->ToggleExpand();
  EXPECT_FALSE(notification_view->IsExpanded());

  // All the fade in animations of views in collapsed state should be performed
  // and recorded here.
  CheckSmoothnessRecorded(
      histograms_collapsed, GetTitleRowDivider(notification_view),
      "Ash.NotificationView.TitleRowDivider.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_collapsed, GetTimestampInCollapsedView(notification_view),
      "Ash.NotificationView.TimestampInTitle.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_collapsed, GetMessageLabel(notification_view),
      "Ash.NotificationView.MessageLabel.FadeIn.AnimationSmoothness");

  base::HistogramTester histograms_expanded;
  notification_view->ToggleExpand();
  EXPECT_TRUE(notification_view->IsExpanded());

  // All the fade in animations of views in expanded state should be performed
  // and recorded here.
  CheckSmoothnessRecorded(
      histograms_expanded, GetHeaderRow(notification_view),
      "Ash.NotificationView.HeaderRow.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_expanded, GetMessageLabelInExpandedState(notification_view),
      "Ash.NotificationView.ExpandedMessageLabel.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_expanded, GetImageContainerView(notification_view),
      "Ash.NotificationView.ImageContainerView.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_expanded, GetActionsRow(notification_view),
      "Ash.NotificationView.ActionsRow.FadeIn.AnimationSmoothness");
}

// TODO(crbug.com/41495194): Re-enable this test
TEST_F(AshNotificationViewTest, ImageExpandCollapseAnimationsRecordSmoothness) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);
  auto notification = CreateTestNotification(/*has_image=*/true,
                                             /*show_snooze_button=*/true);
  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());

  // When we use different images for icon view and image container view, we
  // fade out and scale down image container view when changing to collapsed
  // state. We fade in, scale and translate when changing to expanded state.
  EXPECT_TRUE(notification_view->IsExpanded());
  base::HistogramTester histograms;
  notification_view->ToggleExpand();
  EXPECT_FALSE(notification_view->IsExpanded());

  CheckSmoothnessRecorded(
      histograms, GetImageContainerView(notification_view),
      "Ash.NotificationView.ImageContainerView.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(histograms, GetImageContainerView(notification_view),
                          "Ash.NotificationView.ImageContainerView."
                          "ScaleDown.AnimationSmoothness");

  notification_view->ToggleExpand();
  EXPECT_TRUE(notification_view->IsExpanded());

  CheckSmoothnessRecorded(
      histograms, GetImageContainerView(notification_view),
      "Ash.NotificationView.ImageContainerView.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(histograms, GetImageContainerView(notification_view),
                          "Ash.NotificationView.ImageContainerView."
                          "ScaleAndTranslate.AnimationSmoothness");

  // Clear icon so that icon view and image container view use the same image.
  notification->set_icon(ui::ImageModel());
  message_center::MessageCenter::Get()->UpdateNotification(
      notification->id(), std::move(notification));

  EXPECT_TRUE(notification_view->IsExpanded());

  base::HistogramTester histograms_collapsed;
  notification_view->ToggleExpand();
  EXPECT_FALSE(notification_view->IsExpanded());

  // We scale and translate icon view to collapsed state.
  CheckSmoothnessRecorded(
      histograms_collapsed, GetIconView(notification_view),
      "Ash.NotificationView.IconView.ScaleAndTranslate.AnimationSmoothness");

  base::HistogramTester histograms_expanded;
  notification_view->ToggleExpand();
  EXPECT_TRUE(notification_view->IsExpanded());

  // We scale and translate image container view to expanded state.
  CheckSmoothnessRecorded(histograms_expanded,
                          GetImageContainerView(notification_view),
                          "Ash.NotificationView.ImageContainerView."
                          "ScaleAndTranslate.AnimationSmoothness");
}

TEST_F(AshNotificationViewTest, GroupExpandCollapseAnimationsRecordSmoothness) {
  base::HistogramTester histograms;

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);
  auto notification = CreateTestNotification();
  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());
  MakeNotificationGroupParent(
      notification_view,
      message_center_style::kMaxGroupedNotificationsInCollapsedState);
  EXPECT_FALSE(notification_view->IsExpanded());

  base::HistogramTester histograms_expanded;
  notification_view->ToggleExpand();
  EXPECT_TRUE(notification_view->IsExpanded());

  auto* const expand_button = GetExpandButton(notification_view);

  // All the animations of views in expanded state should be performed and
  // recorded here.
  CheckSmoothnessRecorded(
      histograms_expanded,
      GetCollapsedSummaryView(
          GetFirstGroupedChildNotificationView(notification_view)),
      "Ash.NotificationView.CollapsedSummaryView.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_expanded,
      GetMainView(GetFirstGroupedChildNotificationView(notification_view)),
      "Ash.NotificationView.MainView.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_expanded, expand_button->label(),
      expand_button->GetAnimationHistogramName(
          AshNotificationExpandButton::AnimationType::kFadeOutLabel));
  CheckSmoothnessRecorded(
      histograms_expanded, expand_button,
      expand_button->GetAnimationHistogramName(
          AshNotificationExpandButton::AnimationType::kBoundsChange));

  base::HistogramTester histograms_collapsed;
  notification_view->ToggleExpand();
  EXPECT_FALSE(notification_view->IsExpanded());

  // All the animations of views in collapsed state should be performed and
  // recorded here.
  CheckSmoothnessRecorded(
      histograms_collapsed,
      GetMainView(GetFirstGroupedChildNotificationView(notification_view)),
      "Ash.NotificationView.MainView.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_collapsed,
      GetCollapsedSummaryView(
          GetFirstGroupedChildNotificationView(notification_view)),
      "Ash.NotificationView.CollapsedSummaryView.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms_collapsed, expand_button->label(),
      expand_button->GetAnimationHistogramName(
          AshNotificationExpandButton::AnimationType::kFadeInLabel));
  CheckSmoothnessRecorded(
      histograms_collapsed, expand_button->label(),
      expand_button->GetAnimationHistogramName(
          AshNotificationExpandButton::AnimationType::kBoundsChange));
}

TEST_F(AshNotificationViewTest, SingleToGroupAnimationsRecordSmoothness) {
  base::HistogramTester histograms;

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);
  auto notification = CreateTestNotification();
  notification_center_test_api()->ToggleBubble();

  auto notification1 = CreateTestNotificationInAGroup();

  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification1->id());
  auto notification2 = CreateTestNotificationInAGroup();

  CheckSmoothnessRecorded(
      histograms, GetLeftContent(notification_view),
      "Ash.NotificationView.ConvertSingleToGroup.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms, GetGroupedNotificationsContainer(notification_view),
      "Ash.NotificationView.ConvertSingleToGroup.FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms, GetExpandButton(notification_view)->label(),
      "Ash.NotificationView.ExpandButton.ConvertSingleToGroup."
      "FadeIn.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms, GetExpandButton(notification_view),
      "Ash.NotificationView.ExpandButton.ConvertSingleToGroup."
      "BoundsChange.AnimationSmoothness");
}

TEST_F(AshNotificationViewTest, InlineReplyAnimationsRecordSmoothness) {
  base::HistogramTester histograms;

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);
  auto notification =
      CreateTestNotification(/*has_image=*/true, /*show_snooze_button=*/true);
  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());

  message_center::ButtonInfo info(u"Test button.");
  std::vector<message_center::ButtonInfo> buttons =
      std::vector<message_center::ButtonInfo>(2, info);
  buttons[1].placeholder = std::u16string();
  notification->set_buttons(buttons);
  message_center::MessageCenter::Get()->UpdateNotification(
      notification->id(), std::move(notification));

  // Clicking inline reply button and check animations.
  EXPECT_TRUE(notification_view->IsExpanded());
  views::test::ButtonTestApi test_api(GetActionButtons(notification_view)[1]);
  test_api.NotifyClick(ui::test::TestEvent());

  CheckSmoothnessRecorded(
      histograms, GetActionButtonsRow(notification_view),
      "Ash.NotificationView.ActionButtonsRow.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms, GetInlineReply(notification_view),
      "Ash.NotificationView.InlineReply.FadeIn.AnimationSmoothness");

  // Toggle expand to close inline reply. It should fade out.
  notification_view->ToggleExpand();
  CheckSmoothnessRecorded(
      histograms, GetInlineReply(notification_view),
      "Ash.NotificationView.InlineReply.FadeOut.AnimationSmoothness");
}

TEST_F(AshNotificationViewTest, InlineSettingsAnimationsRecordSmoothness) {
  base::HistogramTester histograms;

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);
  auto notification =
      CreateTestNotification(/*has_image=*/true, /*show_snooze_button=*/true);
  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());

  // Set to collapsed state so that header row will fade out when coming back to
  // main notification view.
  notification_view->SetExpanded(false);

  // Toggle inline settings to access inline settings view.
  ToggleInlineSettings(notification_view);

  // Check fade out views.
  CheckSmoothnessRecorded(
      histograms, GetLeftContent(notification_view),
      "Ash.NotificationView.LeftContent.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms, GetExpandButton(notification_view),
      "Ash.NotificationView.ExpandButton.FadeOut.AnimationSmoothness");
  CheckSmoothnessRecorded(
      histograms, GetIconView(notification_view),
      "Ash.NotificationView.IconView.FadeOut.AnimationSmoothness");

  // Check fade in main right view.
  CheckSmoothnessRecorded(
      histograms, GetMainRightView(notification_view),
      "Ash.NotificationView.MainRightView.FadeIn.AnimationSmoothness");

  // Toggle inline settings again to come back.
  ToggleInlineSettings(notification_view);

  CheckSmoothnessRecorded(
      histograms, GetInlineSettingsRow(notification_view),
      "Ash.NotificationView.InlineSettingsRow.FadeOut.AnimationSmoothness");

  CheckSmoothnessRecorded(
      histograms, GetMainRightView(notification_view),
      "Ash.NotificationView.MainRightView.FadeIn.AnimationSmoothness",
      /*data_point_count=*/2);
}

TEST_F(AshNotificationViewTest,
       GroupNotificationSlideOutAnimationRecordSmoothness) {
  base::HistogramTester histograms;

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto notification = CreateTestNotification();

  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());
  MakeNotificationGroupParent(
      notification_view,
      2 * message_center_style::kMaxGroupedNotificationsInCollapsedState);

  notification_view->ToggleExpand();
  EXPECT_TRUE(notification_view->IsExpanded());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  auto* child_view = GetFirstGroupedChildNotificationView(notification_view);
  notification_view->RemoveGroupNotification(child_view->notification_id());

  base::HistogramTester histogram;

  // The child view should slide out before being deleted and the smoothness
  // should be recorded.
  CheckSmoothnessRecorded(
      histograms, child_view,
      "Ash.Notification.GroupNotification.SlideOut.AnimationSmoothness");
}

TEST_F(AshNotificationViewTest, RecordExpandButtonClickAction) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();
  notification_view()->UpdateWithNotification(*notification);

  notification_view()->SetExpanded(false);
  notification_view()->ToggleExpand();
  histograms.ExpectBucketCount(
      "Ash.NotificationView.ExpandButton.ClickAction",
      metrics_utils::ExpandButtonClickAction::EXPAND_INDIVIDUAL, 1);

  notification_view()->ToggleExpand();
  histograms.ExpectBucketCount(
      "Ash.NotificationView.ExpandButton.ClickAction",
      metrics_utils::ExpandButtonClickAction::COLLAPSE_INDIVIDUAL, 1);

  notification->SetGroupParent();
  notification_view()->UpdateWithNotification(*notification);

  notification_view()->SetExpanded(false);
  notification_view()->ToggleExpand();
  histograms.ExpectBucketCount(
      "Ash.NotificationView.ExpandButton.ClickAction",
      metrics_utils::ExpandButtonClickAction::EXPAND_GROUP, 1);

  notification_view()->ToggleExpand();
  histograms.ExpectBucketCount(
      "Ash.NotificationView.ExpandButton.ClickAction",
      metrics_utils::ExpandButtonClickAction::COLLAPSE_GROUP, 1);
}

TEST_F(AshNotificationViewTest, ExpandButtonAccessibleName) {
  std::u16string notification_title = u"Test title";
  auto notification = CreateTestNotification();
  notification->set_title(notification_title);
  notification_view()->UpdateWithNotification(*notification);
  notification_view()->SetExpanded(false);

  auto* expand_button = notification_view()->expand_button_for_test();

  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP,
                                       notification_title),
            expand_button->GetViewAccessibility().GetCachedName());

  notification_view()->ToggleExpand();
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP,
                                       notification_title),
            expand_button->GetViewAccessibility().GetCachedName());

  // Update the notification title. The expand button tooltip text should be
  // updated accordingly.
  notification_title = u"Updated test title";
  notification->set_title(notification_title);
  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP,
                                       notification_title),
            expand_button->GetViewAccessibility().GetCachedName());
}

TEST_F(AshNotificationViewTest, OnThemeChangedWithoutMessageLabel) {
  EXPECT_NE(nullptr, GetMessageLabel(notification_view()));

  std::unique_ptr<Notification> notification = CreateTestNotification(
      /*has_image=*/false, /*show_snooze_button=*/false, /*has_message=*/true,
      message_center::NOTIFICATION_TYPE_PROGRESS);
  notification_view()->UpdateWithNotification(*notification);
  EXPECT_EQ(nullptr, GetMessageLabel(notification_view()));

  notification = CreateTestNotification(
      /*has_image=*/false, /*show_snooze_button=*/false, /*has_message=*/false);
  notification_view()->UpdateWithNotification(*notification);
  EXPECT_EQ(nullptr, GetMessageLabel(notification_view()));

  // Verify OnThemeChanged doesn't break with a null message_label()
  notification_view()->OnThemeChanged();
  EXPECT_EQ(nullptr, GetMessageLabel(notification_view()));
}

TEST_F(AshNotificationViewTest, DuplicateGroupChildRemovalWithAnimation) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto notification = CreateTestNotification();

  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());
  MakeNotificationGroupParent(
      notification_view,
      2 * message_center_style::kMaxGroupedNotificationsInCollapsedState);

  notification_view->ToggleExpand();
  EXPECT_TRUE(notification_view->IsExpanded());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Ensure a duplicate call to RemoveGroupNotification does not cause a crash.
  auto* child_view = GetFirstGroupedChildNotificationView(notification_view);
  notification_view->RemoveGroupNotification(child_view->notification_id());
  notification_view->RemoveGroupNotification(child_view->notification_id());
}

// Regression test for b/253668543. Ensures toggling the expand state for a
// progress notification with a large image does not result in a crash.
TEST_F(AshNotificationViewTest, CollapseProgressNotificationWithImage) {
  std::unique_ptr<Notification> notification = CreateTestNotification(
      /*has_image=*/true, /*show_snooze_button=*/false, /*has_message=*/false,
      message_center::NOTIFICATION_TYPE_PROGRESS);
  notification_view()->UpdateWithNotification(*notification);

  notification_view()->ToggleExpand();
}

TEST_F(AshNotificationViewTest, ButtonStateUpdated) {
  auto notification = CreateTestNotification();
  notification_center_test_api()->ToggleBubble();

  notification_view()->UpdateWithNotification(*notification);

  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());
  AshNotificationInputContainer* inline_reply =
      static_cast<AshNotificationInputContainer*>(
          GetInlineReply(notification_view));

  EXPECT_TRUE(inline_reply->textfield()->GetText().empty());

  inline_reply->UpdateButtonImage();

  EXPECT_FALSE(inline_reply->button()->GetEnabled());

  inline_reply->textfield()->SetText(u"test");
  inline_reply->UpdateButtonImage();

  EXPECT_TRUE(inline_reply->button()->GetEnabled());
}

// b/269144282: Regression test to make sure the left content container and the
// title row have the same height when the notification is expanded.
TEST_F(AshNotificationViewTest, LeftContentAndTitleRowHeightMatches) {
  auto notification = CreateTestNotification();
  notification_view()->ToggleExpand();
  ASSERT_TRUE(notification_view()->IsExpanded());

  notification->set_icon(ui::ImageModel());
  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(GetLeftContent(notification_view())->height(),
            GetTitleRow(notification_view())->height());

  notification->set_title(u"Camera and microphone are in use.");
  notification_view()->UpdateWithNotification(*notification);

  EXPECT_EQ(GetLeftContent(notification_view())->height(),
            GetTitleRow(notification_view())->height());
}

// AshNotificationLimitTest ----------------------------------------------------

class AshNotificationLimitTest : public AshNotificationViewTestBase,
                                 public testing::WithParamInterface<
                                     /*is_notification_limit_enabled=*/bool> {
 public:
  AshNotificationLimitTest()
      : AshNotificationViewTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureState(features::kNotificationLimit,
                                              GetParam());
  }

  std::unique_ptr<Notification> CreateTestNotificationOfPriority(
      message_center::NotificationPriority priority) {
    return CreateTestNotification(
        /*has_image=*/false, /*show_snooze_button=*/false, /*has_message=*/true,
        message_center::NOTIFICATION_TYPE_SIMPLE, /*image_path=*/std::nullopt,
        /*pinned=*/false, priority);
  }

  std::unique_ptr<Notification> CreateTestPinnedNotification() {
    return CreateTestNotification(
        /*has_image=*/false, /*show_snooze_button=*/false, /*has_message=*/true,
        message_center::NOTIFICATION_TYPE_SIMPLE,
        /*image_path=*/std::nullopt, /*pinned=*/true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  message_center::ScopedNotificationLimitOverrider limit_overrider_{
      kOverridingCountLimit, kOverridingTargetCountAfterRemoval};
};

INSTANTIATE_TEST_SUITE_P(All,
                         AshNotificationLimitTest,
                         /*is_notification_limit_enabled=*/Bool());

// Verifies the feature when all notifications are pinned.
TEST_P(AshNotificationLimitTest, AllNotificationsPinned) {
  std::vector<std::unique_ptr<Notification>> notifications;
  while (notifications.size() < kOverridingCountLimit) {
    notifications.push_back(CreateTestPinnedNotification());
  }

  auto over_limit_notification = CreateTestPinnedNotification();

  // Fast forward to handle the over-limit notification.
  task_environment()->FastForwardBy(kWaitTime);

  // Pinned notifications should be kept regardless of the notification limit
  // feature's enabling state.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetNotifications(),
              UnorderedElementsAre(
                  NotificationIdMatches(notifications[0]->id()),
                  NotificationIdMatches(notifications[1]->id()),
                  NotificationIdMatches(notifications[2]->id()),
                  NotificationIdMatches(notifications[3]->id()),
                  NotificationIdMatches(notifications[4]->id()),
                  NotificationIdMatches(over_limit_notification->id())));
}

// If the notification limit feature is enabled, the pinned notifications should
// not be removed when the notification count is over limit; otherwise, all
// added notifications should exist.
TEST_P(AshNotificationLimitTest, KeepPinnedNotification) {
  // Add a pinned notification.
  std::vector<std::unique_ptr<Notification>> notifications;
  notifications.push_back(CreateTestPinnedNotification());

  // Add a group child notification of a high priority.
  notifications.push_back(
      CreateTestNotificationInAGroup(message_center::HIGH_PRIORITY));

  // Add a pinned notification.
  notifications.push_back(CreateTestPinnedNotification());

  // Keep adding the notifications of high priority until reaching the limit.
  while (notifications.size() < kOverridingCountLimit) {
    notifications.push_back(
        CreateTestNotificationOfPriority(message_center::HIGH_PRIORITY));
  }

  task_environment()->FastForwardBy(kNotificationAgingWaitTime);
  auto over_limit_notification = CreateTestNotification();

  // Fast forward to handle the over-limit notification.
  task_environment()->FastForwardBy(kWaitTime);

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  if (features::IsNotificationLimitEnabled()) {
    // Among the elements of `notifications`, the two pinned ones are kept.
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[2]->id()),
                    NotificationIdMatches(over_limit_notification->id())));
  } else {
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(notifications[2]->id()),
                    NotificationIdMatches(notifications[3]->id()),
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification->id())));
  }
}

// If the notification limit feature is enabled, the notifications with lower
// priorities should be removed when the notification count is over limit;
// otherwise, all added notifications should exist.
TEST_P(AshNotificationLimitTest, LimitByPriorityOrder) {
  // Add two notifications with a high priority.
  std::vector<std::unique_ptr<Notification>> notifications;
  ASSERT_GT(kOverridingCountLimit, 2u);
  while (notifications.size() < 2) {
    notifications.push_back(
        CreateTestNotificationOfPriority(message_center::HIGH_PRIORITY));
  }

  // Keep adding notifications of a default priority until reaching the limit.
  while (notifications.size() < kOverridingCountLimit) {
    notifications.push_back(CreateTestNotification());
  }

  task_environment()->FastForwardBy(kNotificationAgingWaitTime);
  auto over_limit_notification = CreateTestNotification();

  // Fast forward to handle the over-limit notification.
  task_environment()->FastForwardBy(kWaitTime);

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  if (features::IsNotificationLimitEnabled()) {
    // Among the elements of `notifications`, those of high priority are kept.
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(over_limit_notification->id())));
  } else {
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(notifications[2]->id()),
                    NotificationIdMatches(notifications[3]->id()),
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification->id())));
  }
}

// If the notification limit feature is enabled and the notification count is
// over limit, when deciding the notifications to remove among those of the
// same priority, the oldest ones should be removed.
TEST_P(AshNotificationLimitTest, LimitByPriorityTimestampOrder) {
  std::vector<std::unique_ptr<Notification>> notifications;
  notifications.push_back(CreateTestNotification());
  notifications.push_back(
      CreateTestNotificationOfPriority(message_center::HIGH_PRIORITY));

  while (notifications.size() < kOverridingCountLimit) {
    // Fast forward to ensure a larger timestamp.
    task_environment()->FastForwardBy(kWaitTime);

    notifications.push_back(CreateTestNotification());
  }

  task_environment()->FastForwardBy(kNotificationAgingWaitTime);
  auto over_limit_notification =
      CreateTestNotificationOfPriority(message_center::HIGH_PRIORITY);

  // Fast forward to handle the over-limit notification.
  task_environment()->FastForwardBy(kWaitTime);

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  if (features::IsNotificationLimitEnabled()) {
    // Among the elements in `notifications` of the default priority, the last
    // one is kept because it has the largest timestamp.
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification->id())));
  } else {
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(notifications[2]->id()),
                    NotificationIdMatches(notifications[3]->id()),
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification->id())));
  }
}

// If the notification limit feature is enabled, among the notifications of the
// same priority, the oldest ones should be removed; otherwise, all added
// notifications should exist.
TEST_P(AshNotificationLimitTest, LimitByTimestampOrder) {
  std::vector<std::unique_ptr<Notification>> notifications;
  while (notifications.size() < kOverridingCountLimit) {
    notifications.push_back(CreateTestNotification());
    // Ensure the members of `notifications` have increasing timestamps.
    task_environment()->FastForwardBy(kWaitTime);
  }

  task_environment()->FastForwardBy(kNotificationAgingWaitTime);
  auto over_limit_notification1 = CreateTestNotification();
  auto over_limit_notification2 = CreateTestNotification();

  // Fast forward to handle the over-limit notification.
  task_environment()->FastForwardBy(kWaitTime);

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  if (features::IsNotificationLimitEnabled()) {
    // Among the elements of `notifications`, the last one is kept because it
    // has the largest timestamp.
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification1->id()),
                    NotificationIdMatches(over_limit_notification2->id())));
  } else {
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(notifications[2]->id()),
                    NotificationIdMatches(notifications[3]->id()),
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification1->id()),
                    NotificationIdMatches(over_limit_notification2->id())));
  }
}

// If the notification limit feature is enabled, the most recent notifications
// should be kept regardless of their priorities.
TEST_P(AshNotificationLimitTest, MostRecentNotifications) {
  std::vector<std::unique_ptr<Notification>> notifications;
  while (notifications.size() < kOverridingCountLimit) {
    notifications.push_back(
        CreateTestNotificationOfPriority(message_center::MAX_PRIORITY));
  }

  task_environment()->FastForwardBy(kNotificationAgingWaitTime);
  auto over_limit_notification1 = CreateTestNotification();
  auto over_limit_notification2 =
      CreateTestNotificationOfPriority(message_center::LOW_PRIORITY);
  auto over_limit_notification3 =
      CreateTestNotificationOfPriority(message_center::MIN_PRIORITY);

  // Fast forward to handle the over-limit notification.
  task_environment()->FastForwardBy(kWaitTime);

  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  if (features::IsNotificationLimitEnabled()) {
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(over_limit_notification1->id()),
                    NotificationIdMatches(over_limit_notification2->id()),
                    NotificationIdMatches(over_limit_notification3->id())));
  } else {
    EXPECT_THAT(message_center->GetNotifications(),
                UnorderedElementsAre(
                    NotificationIdMatches(notifications[0]->id()),
                    NotificationIdMatches(notifications[1]->id()),
                    NotificationIdMatches(notifications[2]->id()),
                    NotificationIdMatches(notifications[3]->id()),
                    NotificationIdMatches(notifications[4]->id()),
                    NotificationIdMatches(over_limit_notification1->id()),
                    NotificationIdMatches(over_limit_notification2->id()),
                    NotificationIdMatches(over_limit_notification3->id())));
  }
}

// AshNotificationViewDragTestBase ---------------------------------------------

class AshNotificationViewDragTestBase : public AshNotificationViewTestBase {
 public:
  // AshNotificationViewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kNotificationImageDrag, true}});

    AshNotificationViewTestBase::SetUp();
    notification_test_api_ = std::make_unique<NotificationCenterTestApi>();

    // Configure the widget that handles notification drop.
    drop_handling_widget_ = CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        /*delegate=*/nullptr, desks_util::GetActiveDeskContainerId(),
        /*bounds=*/gfx::Rect(100, 100, 300, 300), /*show=*/true);
    aura::client::SetDragDropDelegate(drop_handling_widget_->GetNativeView(),
                                      &drag_drop_delegate_);
  }

  // Returns the center of drag area of `notification_view` in screen
  // coordinates.
  gfx::Point GetDragAreaCenterInScreen(
      const AshNotificationView& notification_view) {
    const std::optional<gfx::Rect> drag_area_bounds =
        notification_view.GetDragAreaBounds();
    EXPECT_TRUE(drag_area_bounds);
    gfx::Rect drag_area_in_screen = *drag_area_bounds;
    views::View::ConvertRectToScreen(&notification_view, &drag_area_in_screen);
    return drag_area_in_screen.CenterPoint();
  }

  // Drags from `start_point` then drops at `drop_point`. `start_point` and
  // `drop_point` are in screen coordinates.
  void PerformDragAndDrop(const gfx::Point& start_point,
                          const gfx::Point& drop_point) {
    base::RunLoop run_loop;
    ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
        base::BindLambdaForTesting([&]() {
          // Move mouse/touch to `target_point` then release.
          if (DoesUseGesture()) {
            GetEventGenerator()->MoveTouch(drop_point);
            GetEventGenerator()->ReleaseTouch();
          } else {
            GetEventGenerator()->MoveMouseTo(drop_point);
            GetEventGenerator()->ReleaseLeftButton();
          }
        }),
        run_loop.QuitClosure());

    StartDragAt(start_point);
    run_loop.Run();
  }

  // Starts drag at the specified location.
  void StartDragAt(const gfx::Point& point_in_screen) {
    if (DoesUseGesture()) {
      // Press touch to trigger notification drag.
      GetEventGenerator()->PressTouch(point_in_screen);
    } else {
      // Press the mouse then move to trigger notification drag.
      GetEventGenerator()->MoveMouseTo(point_in_screen);
      GetEventGenerator()->PressLeftButton();
      MoveDragByOneStep();
    }
  }

  // Drags and drops `notification_view`. If `drag_to_widget` is true,
  // `notification_view` is dropped on the center of `drop_handling_widget_`;
  // otherwise, `notification_view` is dropped on the location outside of
  // `drop_handling_widget_`, which means that dropped data is not handled.
  void DragAndDropNotification(const AshNotificationView& notification_view,
                               bool drag_to_widget) {
    PerformDragAndDrop(
        GetDragAreaCenterInScreen(notification_view),
        (drag_to_widget ? GetDropHandlingWidgetCenter()
                        : GetPrimaryDisplay().bounds().left_center()));
  }

  // Returns the notification view corresponding to the given notification id.
  // `id` can refer to either a popup notification view or a message center
  // notification view.
  AshNotificationView* GetViewForNotificationId(const std::string& id) {
    if (IsPopupNotification()) {
      // Get the notification view from the message popup collection.
      auto* popup_view = static_cast<message_center::MessagePopupView*>(
          notification_test_api_->GetPopupViewForId(id));
      DCHECK(popup_view);
      return static_cast<AshNotificationView*>(popup_view->message_view());
    }

    // Get the notification view from the message center bubble.
    return static_cast<AshNotificationView*>(
        notification_test_api_->GetNotificationViewForId(id));
  }

  // Returns true when using gesture drag rather than mouse drag, specified
  // by the test params; otherwise, returns false.
  virtual bool DoesUseGesture() const = 0;

  // Returns true when using the popup notification rather than the tray
  // notification. specified by the test params; otherwise, returns false.
  virtual bool IsPopupNotification() const = 0;

  gfx::Point GetDropHandlingWidgetCenter() const {
    return drop_handling_widget_->GetWindowBoundsInScreen().CenterPoint();
  }

  const MockAshNotificationDragDropDelegate& drag_drop_delegate() const {
    return drag_drop_delegate_;
  }

  NotificationCenterTestApi* notification_test_api() {
    return notification_test_api_.get();
  }

 private:
  // Moves drag by one step.
  void MoveDragByOneStep() {
    // The move distance for each drag move.
    constexpr int kMoveDistancePerStep = 10;

    if (DoesUseGesture()) {
      GetEventGenerator()->MoveTouchBy(-kMoveDistancePerStep, /*y=*/0);
    } else {
      GetEventGenerator()->MoveMouseBy(-kMoveDistancePerStep, /*y=*/0);
    }
  }

  std::unique_ptr<NotificationCenterTestApi> notification_test_api_;

  // A custom widget to handle notification image drop.
  std::unique_ptr<views::Widget> drop_handling_widget_;
  MockAshNotificationDragDropDelegate drag_drop_delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// The test class that checks the notification drag feature with both mouse drag
// and gesture drag.
class AshNotificationViewDragTest
    : public AshNotificationViewDragTestBase,
      public testing::WithParamInterface<std::tuple<
          /*use_gesture=*/bool,
          /*is_popup=*/bool,
          /*is_image_file_backed=*/bool,
          /*dropped_to_widget=*/bool>> {
 public:
  // AshNotificationViewDragTestBase:
  bool DoesUseGesture() const override { return std::get<0>(GetParam()); }
  bool IsPopupNotification() const override { return std::get<1>(GetParam()); }

  // Returns true if the notification image is backed by a file.
  bool IsImageFileBacked() const { return std::get<2>(GetParam()); }

  // Returns true if the notification image should be dropped on the drop
  // handling widget. If the return is false, notification drop is not handled.
  bool DroppedToWidget() const { return std::get<3>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AshNotificationViewDragTest,
    testing::Combine(/*use_gesture=*/testing::Bool(),
                     /*is_popup=*/testing::Bool(),
                     /*is_image_file_backed=*/testing::Bool(),
                     /*dropped_to_widget=*/testing::Bool()));

// Verifies the drag-and-drop of an orindary notification view.
TEST_P(AshNotificationViewDragTest, Basics) {
  // Add an image notification.
  std::optional<base::FilePath> image_file_path;
  if (IsImageFileBacked()) {
    // Use a dummy file path for the file-backed image notification.
    image_file_path.emplace("dummy_path.png");
  }
  std::unique_ptr<Notification> notification = CreateTestNotification(
      /*has_image=*/true, /*show_snooze_button=*/false, /*has_message=*/false,
      message_center::NOTIFICATION_TYPE_SIMPLE, image_file_path);

  if (IsPopupNotification()) {
    // Wait until the notification popup shows.
    MessagePopupAnimationWaiter(
        GetPrimaryNotificationCenterTray()->popup_collection())
        .Wait();
    EXPECT_FALSE(
        message_center::MessageCenter::Get()->GetPopupNotifications().empty());
  } else {
    notification_test_api()->ToggleBubble();
    EXPECT_TRUE(message_center::MessageCenter::Get()->IsMessageCenterVisible());
  }

  if (DroppedToWidget()) {
    // Verify the image drop is handled if the notification is dragged to the
    // widget.
    if (IsImageFileBacked()) {
      EXPECT_CALL(drag_drop_delegate(), HandleFilePathData(*image_file_path));
    } else {
      EXPECT_CALL(drag_drop_delegate(), HandleHtmlData);
    }
  }
  base::HistogramTester tester;
  DragAndDropNotification(*GetViewForNotificationId(notification->id()),
                          DroppedToWidget());

  // Check the notification catalog name.
  tester.ExpectBucketCount("Ash.NotificationView.ImageDrag.Start",
                           NotificationCatalogName::kNone, 1);
  tester.ExpectBucketCount(
      "Ash.NotificationView.ImageDrag.EndState",
      DroppedToWidget()
          ? AshNotificationDragController::DragEndState::kCompletedWithDrop
          : AshNotificationDragController::DragEndState::kCompletedWithoutDrop,
      1);

  // The the message center bubble is closed and the popup notification is
  // dismissed when drag ends.
  MessageCenterTargetVisibilityWaiter(/*target_visible=*/false).Wait();
  EXPECT_TRUE(
      message_center::MessageCenter::Get()->GetPopupNotifications().empty());

  // The dragged notification should be removed from the message center if the
  // notification is dropped on the drop handling widget.
  const bool has_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification->id());
  EXPECT_EQ(has_notification, !DroppedToWidget());
}

// Verifies the drag-and-drop of a grouped notification view.
TEST_P(AshNotificationViewDragTest, GroupedNotification) {
  // Add two image notification views belonging to the same group.
  std::optional<base::FilePath> image_file_path;
  if (IsImageFileBacked()) {
    // Use a dummy file path for the file-backed image notification.
    image_file_path.emplace("dummy_path.png");
  }
  std::unique_ptr<Notification> notification = CreateTestNotificationInAGroup(
      /*has_image=*/true, image_file_path);
  CreateTestNotificationInAGroup(/*has_image=*/true, image_file_path);

  // Get the id of the group parent notification.
  const std::string parent_notification_id =
      message_center::MessageCenter::Get()
          ->FindParentNotification(notification.get())
          ->id();

  if (IsPopupNotification()) {
    // Wait until the notification popup shows.
    MessagePopupAnimationWaiter(
        GetPrimaryNotificationCenterTray()->popup_collection())
        .Wait();
  } else {
    // Show the message center bubble.
    notification_test_api()->ToggleBubble();
  }

  // Expand the parent notification.
  AshNotificationView* group_parent_view =
      GetViewForNotificationId(parent_notification_id);
  group_parent_view->ToggleExpand();

  // Expand the first child notification. Ensure the expansion animation ends.
  AshNotificationView* child_view =
      GetFirstGroupedChildNotificationView(group_parent_view);
  const std::string child_notification_id = child_view->notification_id();
  child_view->ToggleExpand();
  if (IsPopupNotification()) {
    MessagePopupAnimationWaiter(
        GetPrimaryNotificationCenterTray()->popup_collection())
        .Wait();
  } else {
    notification_test_api()->CompleteNotificationListAnimation();
  }

  if (DroppedToWidget()) {
    // Drag `child_view` to the widget. Verify the image drop is handled.
    if (IsImageFileBacked()) {
      EXPECT_CALL(drag_drop_delegate(), HandleFilePathData(*image_file_path));
    } else {
      EXPECT_CALL(drag_drop_delegate(), HandleHtmlData);
    }
  }
  DragAndDropNotification(*child_view, DroppedToWidget());

  // The the message center bubble is closed and the popup notification is
  // dismissed when drag ends.
  MessageCenterTargetVisibilityWaiter(/*target_visible=*/false).Wait();
  EXPECT_TRUE(
      message_center::MessageCenter::Get()->GetPopupNotifications().empty());

  // The dragged child notification should be removed from the message center if
  // the notification is dropped on the drop handling widget.
  const bool has_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          child_notification_id);
  EXPECT_EQ(has_notification, !DroppedToWidget());
}

// Tests notification async drop.
class AshNotificationViewDragAsyncDropTest
    : public AshNotificationViewDragTestBase,
      public testing::WithParamInterface<std::tuple<
          /*use_gesture=*/bool,
          /*is_popup=*/bool,
          /*allow_to_drop=*/bool>> {
 public:
  // AshNotificationViewDragTestBase:
  void SetUp() override {
    AshNotificationViewDragTestBase::SetUp();

    // Enlarge the display to show multiple image notifications.
    UpdateDisplay("0+0-1200x800");
  }
  bool DoesUseGesture() const override { return std::get<0>(GetParam()); }
  bool IsPopupNotification() const override { return std::get<1>(GetParam()); }

  // Returns true if `dlp_controller_` allows to drop data.
  bool AllowToDrop() const { return std::get<2>(GetParam()); }

  // Adds one image notification and waits for the corresponding notification
  // view to show. Returns the added notification.
  std::unique_ptr<Notification> AddImageNotificationAndWaitForShow() {
    std::unique_ptr<Notification> notification = CreateTestNotification(
        /*has_image=*/true, /*show_snooze_button=*/false, /*has_message=*/false,
        message_center::NOTIFICATION_TYPE_SIMPLE);

    if (IsPopupNotification()) {
      // Wait until the notification popup shows.
      MessagePopupAnimationWaiter(
          GetPrimaryNotificationCenterTray()->popup_collection())
          .Wait();
      EXPECT_FALSE(message_center::MessageCenter::Get()
                       ->GetPopupNotifications()
                       .empty());
    } else {
      notification_test_api()->ToggleBubble();
      EXPECT_TRUE(
          message_center::MessageCenter::Get()->IsMessageCenterVisible());
    }
    return notification;
  }

  ui::MockDataTransferPolicyController dlp_controller_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AshNotificationViewDragAsyncDropTest,
                         testing::Combine(/*use_gesture=*/testing::Bool(),
                                          /*is_popup=*/testing::Bool(),
                                          /*allow_to_drop=*/testing::Bool()));

TEST_P(AshNotificationViewDragAsyncDropTest, Basics) {
  // Configure `dlp_controller_` to hold the drop callback. `drop_callback` will
  // run later if drop is allowed.
  base::OnceClosure drop_callback;
  EXPECT_CALL(dlp_controller_, DropIfAllowed(_, _, _, _))
      .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb) {
        drop_callback = std::move(drop_cb);
      });

  std::unique_ptr<Notification> notification =
      AddImageNotificationAndWaitForShow();
  DragAndDropNotification(*GetViewForNotificationId(notification->id()),
                          /*drag_to_widget*/ true);

  if (AllowToDrop()) {
    // Run `drop_callback` after `DragNotificationToWidget()` to emulate an
    // async drop.
    std::move(drop_callback).Run();
  } else {
    // Reset `drop_callback` to trigger drag cancellation.
    drop_callback.Reset();
  }

  // The dragged notification should be removed from the message center if drop
  // is allowed.
  const bool has_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification->id());
  EXPECT_EQ(has_notification, !AllowToDrop());
}

// Tests when an async drop is interrupted by a notification drag.
TEST_P(AshNotificationViewDragAsyncDropTest,
       InterruptAsyncDropWithNotificationDrag) {
  base::OnceClosure first_drop_callback;
  base::OnceClosure second_drop_callback;

  {
    // Configure `dlp_controller_` to hold all drop callbacks.
    testing::InSequence s;
    EXPECT_CALL(dlp_controller_, DropIfAllowed(_, _, _, _))
        .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                      std::optional<ui::DataTransferEndpoint> data_dst,
                      std::optional<std::vector<ui::FileInfo>> filenames,
                      base::OnceClosure drop_cb) {
          first_drop_callback = std::move(drop_cb);
        });
    EXPECT_CALL(dlp_controller_, DropIfAllowed(_, _, _, _))
        .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                      std::optional<ui::DataTransferEndpoint> data_dst,
                      std::optional<std::vector<ui::FileInfo>> filenames,
                      base::OnceClosure drop_cb) {
          second_drop_callback = std::move(drop_cb);
        });
  }

  // Add one image notification then perform drag-and-drop.
  std::unique_ptr<Notification> notification1 =
      AddImageNotificationAndWaitForShow();
  DragAndDropNotification(*GetViewForNotificationId(notification1->id()),
                          /*drag_to_widget*/ true);

  // Wait until the message bubble is closed and the popup is hidden.
  MessageCenterTargetVisibilityWaiter(/*target_visible=*/false).Wait();
  EXPECT_TRUE(
      message_center::MessageCenter::Get()->GetPopupNotifications().empty());

  // Start the second notification drag before the active async drop completes.
  std::unique_ptr<Notification> notification2 =
      AddImageNotificationAndWaitForShow();
  DragAndDropNotification(*GetViewForNotificationId(notification2->id()),
                          /*drag_to_widget*/ true);

  // Run `first_drop_callback` or not depending on `AllowToDrop()`.
  if (AllowToDrop()) {
    std::move(first_drop_callback).Run();
  } else {
    first_drop_callback.Reset();
  }

  // Always run `second_drop_callback`.
  std::move(second_drop_callback).Run();

  // Because the async drop is interrupted by a new notification drag,
  // `notification1` still exists.
  const bool has_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification1->id());
  EXPECT_TRUE(has_notification);
}

// Tests when an async drop is interrupted by a view drag.
TEST_P(AshNotificationViewDragAsyncDropTest, InterruptAsyncDropWithViewDrag) {
  base::OnceClosure first_drop_callback;
  base::OnceClosure second_drop_callback;

  {
    // Configure `dlp_controller_` to hold all drop callbacks.
    testing::InSequence s;
    EXPECT_CALL(dlp_controller_, DropIfAllowed(_, _, _, _))
        .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                      std::optional<ui::DataTransferEndpoint> data_dst,
                      std::optional<std::vector<ui::FileInfo>> filenames,
                      base::OnceClosure drop_cb) {
          first_drop_callback = std::move(drop_cb);
        });
    EXPECT_CALL(dlp_controller_, DropIfAllowed(_, _, _, _))
        .WillOnce([&](std::optional<ui::DataTransferEndpoint> data_src,
                      std::optional<ui::DataTransferEndpoint> data_dst,
                      std::optional<std::vector<ui::FileInfo>> filenames,
                      base::OnceClosure drop_cb) {
          second_drop_callback = std::move(drop_cb);
        });
  }

  // Add one image notification then perform drag-and-drop.
  std::unique_ptr<Notification> notification =
      AddImageNotificationAndWaitForShow();
  DragAndDropNotification(*GetViewForNotificationId(notification->id()),
                          /*drag_to_widget*/ true);

  // Wait until the message bubble is closed and the popup is hidden.
  MessageCenterTargetVisibilityWaiter(/*target_visible=*/false).Wait();
  EXPECT_TRUE(
      message_center::MessageCenter::Get()->GetPopupNotifications().empty());

  // Create a separate draggable widget.
  std::unique_ptr<views::Widget> draggable_widget = CreateFramelessTestWidget();
  draggable_widget->SetBounds(
      {/*origin=*/gfx::Point(600, 600), /*size=*/gfx::Size(100, 100)});
  draggable_widget->SetContentsView(
      std::make_unique<DraggableTestView>(/*set_drag_image=*/true,
                                          /*set_file_name=*/true));
  draggable_widget->Show();

  // Drag-and-drop `draggable_widget`.
  PerformDragAndDrop(draggable_widget->GetWindowBoundsInScreen().CenterPoint(),
                     GetDropHandlingWidgetCenter());

  // Run `first_drop_callback` or not depending on `AllowToDrop()`.
  if (AllowToDrop()) {
    std::move(first_drop_callback).Run();
  } else {
    first_drop_callback.Reset();
  }

  // Always run `second_drop_callback`.
  std::move(second_drop_callback).Run();

  // Because the async drop is interrupted by a new drag, `notification` should
  // still exist.
  const bool has_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification->id());
  EXPECT_TRUE(has_notification);
}

// Checks drag-and-drop on a screen capture notification view.
class ScreenCaptureNotificationViewDragTest
    : public AshNotificationViewDragTestBase,
      public testing::WithParamInterface<std::tuple<
          /*use_gesture=*/bool,
          /*is_popup=*/bool>> {
 public:
  // AshNotificationViewDragTestBase:
  bool DoesUseGesture() const override { return std::get<0>(GetParam()); }
  bool IsPopupNotification() const override { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ScreenCaptureNotificationViewDragTest,
                         testing::Combine(/*use_gesture=*/testing::Bool(),
                                          /*is_popup=*/testing::Bool()));

// Verifies drag-and-drop on a screen capture notification. NOTE: a screen
// capture notification's image is always file-backed.
TEST_P(ScreenCaptureNotificationViewDragTest, Basics) {
  // Take a full screenshot then wait for the file path to the saved image.
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  controller->PerformCapture();
  const base::FilePath image_file_path = WaitForCaptureFileToBeSaved();

  // Get the notification view.
  if (IsPopupNotification()) {
    // Wait until the notification popup shows.
    MessagePopupAnimationWaiter(
        GetPrimaryNotificationCenterTray()->popup_collection())
        .Wait();
    EXPECT_FALSE(
        message_center::MessageCenter::Get()->GetPopupNotifications().empty());
  } else {
    notification_test_api()->ToggleBubble();
    EXPECT_TRUE(message_center::MessageCenter::Get()->IsMessageCenterVisible());
  }

  // Drag to the center of `widget` then release. Verify that the screenshot
  // image carried by the drag data is handled.
  EXPECT_CALL(drag_drop_delegate(), HandleFilePathData(image_file_path));
  base::HistogramTester tester;
  DragAndDropNotification(
      *GetViewForNotificationId(kScreenCaptureNotificationId),
      /*drag_to_widget=*/true);

  // Check the notification catalog name.
  tester.ExpectBucketCount("Ash.NotificationView.ImageDrag.Start",
                           NotificationCatalogName::kScreenCapture, 1);
}

class DragAfterNotificationRemovalTest
    : public AshNotificationViewDragTestBase {
 private:
  // AshNotificationViewDragTestBase:
  bool DoesUseGesture() const override { return false; }
  bool IsPopupNotification() const override { return true; }
};

// Verifies that removing a notification then dragging its corresponding view
// shortly after removal works as expected.
TEST_F(DragAfterNotificationRemovalTest, Basics) {
  std::unique_ptr<Notification> notification = CreateTestNotification(
      /*has_image=*/true, /*show_snooze_button=*/false, /*has_message=*/false,
      message_center::NOTIFICATION_TYPE_SIMPLE,
      std::make_optional<base::FilePath>("dummy_file_path"));

  // Wait until the notification popup shows.
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->GetPopupNotifications().empty());

  // Remove `notification` from the message center.
  message_center::MessageCenter::Get()->RemoveNotification(notification->id(),
                                                           /*by_user=*/true);
  EXPECT_TRUE(
      message_center::MessageCenter::Get()->GetPopupNotifications().empty());

  // Drag the view corresponding to `notification`. Note that at this moment
  // `notification_view` still exists due to the fade-out animation.
  const AshNotificationView* const notification_view =
      GetViewForNotificationId(notification->id());
  ASSERT_TRUE(notification_view);
  StartDragAt(GetDragAreaCenterInScreen(*notification_view));

  // Drag should NOT start.
  EXPECT_FALSE(notification_view->GetWidget()->dragged_view());
}

}  // namespace ash

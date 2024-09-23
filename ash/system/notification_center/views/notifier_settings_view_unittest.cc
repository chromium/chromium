// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notifier_settings_view.h"

#include <stddef.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/notifier_settings_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/test_notifier_settings_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

using message_center::NotifierId;

class NotifierSettingsViewTest : public AshTestBase {
 public:
  NotifierSettingsViewTest();

  NotifierSettingsViewTest(const NotifierSettingsViewTest&) = delete;
  NotifierSettingsViewTest& operator=(const NotifierSettingsViewTest&) = delete;

  ~NotifierSettingsViewTest() override;

  void SetNoNotifiers(bool no_notifiers) {
    ash_test_helper()->notifier_settings_controller()->set_no_notifiers(
        no_notifiers);
  }
};

NotifierSettingsViewTest::NotifierSettingsViewTest() = default;

NotifierSettingsViewTest::~NotifierSettingsViewTest() = default;

TEST_F(NotifierSettingsViewTest, TestEmptyNotifierView) {
  SetNoNotifiers(false);
  auto notifier_settings_view = std::make_unique<NotifierSettingsView>();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(notifier_settings_view->no_notifiers_view_->GetVisible());
  EXPECT_TRUE(notifier_settings_view->top_label_->GetVisible());

  SetNoNotifiers(true);
  notifier_settings_view = std::make_unique<NotifierSettingsView>();
  // Wait for mojo.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(notifier_settings_view->no_notifiers_view_->GetVisible());
  EXPECT_FALSE(notifier_settings_view->top_label_->GetVisible());
}

TEST_F(NotifierSettingsViewTest, AccessibleProperties) {
  auto notifier_settings_view = std::make_unique<NotifierSettingsView>();
  ui::AXNodeData data;

  notifier_settings_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kList);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
}

// Tests the notifier settings view with kSettingsAppNotificationSettings
// enabled/disabled.
class NotifierSettingsViewSettingsAppNotificationTest
    : public NotifierSettingsViewTest,
      public testing::WithParamInterface<bool> {
 public:
  NotifierSettingsViewSettingsAppNotificationTest() = default;
  NotifierSettingsViewSettingsAppNotificationTest(
      const NotifierSettingsViewSettingsAppNotificationTest&) = delete;
  NotifierSettingsViewSettingsAppNotificationTest& operator=(
      const NotifierSettingsViewSettingsAppNotificationTest&) = delete;
  ~NotifierSettingsViewSettingsAppNotificationTest() = default;

  void SetUp() override {
    feature_list_.InitWithFeatureState(
        features::kSettingsAppNotificationSettings, GetParam());
    NotifierSettingsViewTest::SetUp();
  }

  bool IsSettingsAppNotificationSettingsEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NotifierSettingsViewSettingsAppNotificationTest,
                         testing::Bool());

TEST_P(NotifierSettingsViewSettingsAppNotificationTest,
       NotificationSettingsLabelTest) {
  auto notifier_settings_view = std::make_unique<NotifierSettingsView>();
  EXPECT_TRUE(notifier_settings_view->get_quiet_mode_icon_view_for_test());
  EXPECT_TRUE(notifier_settings_view->get_quiet_mode_toggle_for_test());
  EXPECT_EQ(
      IsSettingsAppNotificationSettingsEnabled(),
      !!notifier_settings_view->get_notification_settings_lable_for_test());
  EXPECT_NE(IsSettingsAppNotificationSettingsEnabled(),
            !!notifier_settings_view->get_scroller_view_for_test());
}

}  // namespace ash

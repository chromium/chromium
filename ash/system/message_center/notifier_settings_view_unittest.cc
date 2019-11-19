// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notifier_settings_view.h"

#include <stddef.h>

#include <memory>

#include "ash/public/cpp/notifier_settings_controller.h"
#include "ash/shell.h"
#include "ash/system/message_center/test_notifier_settings_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/macros.h"
#include "base/run_loop.h"

namespace ash {

using message_center::NotifierId;

class NotifierSettingsViewTest : public AshTestBase {
 public:
  NotifierSettingsViewTest();
  ~NotifierSettingsViewTest() override;

  void SetNoNotifiers(bool no_notifiers) {
    ash_test_helper()->notifier_settings_controller()->set_no_notifiers(
        no_notifiers);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsViewTest);
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

}  // namespace ash

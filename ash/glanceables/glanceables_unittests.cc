// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_restore_view.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/glanceables/test_glanceables_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

// Unified test suite for the glanceables controller, views, etc.
//
// Use a "no session" test so the glanceables widget is not automatically
// created at the start of the test.
// TODO(crbug.com/1353119): Once glanceables are shown by code in the
// chrome/browser/ash layer, switch this to AshTestBase.
class GlanceablesTest : public NoSessionAshTestBase {
 public:
  GlanceablesTest() = default;
  ~GlanceablesTest() override = default;

  // testing::Test:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    controller_ = Shell::Get()->glanceables_controller();
  }

  TestGlanceablesDelegate* GetTestDelegate() {
    return static_cast<TestGlanceablesDelegate*>(controller_->delegate_.get());
  }

  GlanceablesRestoreView* GetRestoreView() {
    return controller_->view_->restore_view_;
  }

 protected:
  GlanceablesController* controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_{features::kGlanceables};
};

TEST_F(GlanceablesTest, ClickOnSessionRestore) {
  controller_->CreateUi();

  GlanceablesRestoreView* restore_view = GetRestoreView();
  ASSERT_TRUE(restore_view);
  ASSERT_EQ(0, GetTestDelegate()->restore_session_count());

  // Click on the restore view (which is a button).
  views::test::ButtonTestApi(restore_view).NotifyClick(ui::test::TestEvent());

  EXPECT_EQ(1, GetTestDelegate()->restore_session_count());
}

}  // namespace ash

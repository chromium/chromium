// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"
#include <memory>

#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_observer.h"

namespace ash {

class UnifiedSystemTrayControllerTest : public AshTestBase {
 public:
  UnifiedSystemTrayControllerTest() = default;
  UnifiedSystemTrayControllerTest(const UnifiedSystemTrayControllerTest&) =
      delete;
  UnifiedSystemTrayControllerTest& operator=(
      const UnifiedSystemTrayControllerTest&) = delete;
  ~UnifiedSystemTrayControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();
    AshTestBase::SetUp();
    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();

    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
  }

  void TearDown() override {
    DCHECK(quick_settings_view_);
    widget_.reset();
    quick_settings_view_ = nullptr;
    controller_.reset();
    model_.reset();

    AshTestBase::TearDown();
  }

  void InitializeQuickSettingsView() {
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    quick_settings_view_ =
        widget_->SetContentsView(controller_->CreateQuickSettingsView(600));
  }

  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget_`.
  raw_ptr<QuickSettingsView, DanglingUntriaged | ExperimentalAsh>
      quick_settings_view_;
};

// Tests that setting the `UnifiedSystemTrayModel::StateOnOpen` pref to
// collapsed is a no-op.
TEST_F(UnifiedSystemTrayControllerTest, ExpandedPrefIsNoOp) {
  // Set the pref to collapsed, there should be no effect.
  model_->set_expanded_on_open(UnifiedSystemTrayModel::StateOnOpen::COLLAPSED);

  InitializeQuickSettingsView();

  EXPECT_TRUE(model_->IsExpandedOnOpen());
}

}  // namespace ash

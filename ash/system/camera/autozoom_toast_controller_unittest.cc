// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_toast_controller.h"

#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class TestDelegate : public AutozoomToastController::Delegate {
 public:
  void AddAutozoomObserver(AutozoomObserver* observer) override {
    ASSERT_EQ(autozoom_observer, nullptr);
    autozoom_observer = observer;
  }

  void RemoveAutozoomObserver(AutozoomObserver* observer) override {
    ASSERT_EQ(autozoom_observer, observer);
    autozoom_observer = nullptr;
  }

  bool IsAutozoomEnabled() override { return autozoom_enabled_; }

  bool IsAutozoomControlEnabled() override { return autozoom_control_enabled_; }

  void SetAutozoomEnabled(bool autozoom_enabled) {
    autozoom_enabled_ = autozoom_enabled;
    if (autozoom_observer != nullptr) {
      autozoom_observer->OnAutozoomStateChanged(
          autozoom_enabled_ ? cros::mojom::CameraAutoFramingState::ON_SINGLE
                            : cros::mojom::CameraAutoFramingState::OFF);
    }
  }

  void SetAutozoomControlEnabled(bool autozoom_control_enabled) {
    autozoom_control_enabled_ = autozoom_control_enabled;
    if (autozoom_observer != nullptr) {
      autozoom_observer->OnAutozoomControlEnabledChanged(
          autozoom_control_enabled_);
    }
  }

  raw_ptr<AutozoomObserver> autozoom_observer = nullptr;

 private:
  bool autozoom_enabled_ = false;
  bool autozoom_control_enabled_ = false;
};

class AutozoomToastControllerTest : public AshTestBase {
 public:
  AutozoomToastControllerTest() = default;
  ~AutozoomToastControllerTest() override = default;

  AutozoomToastControllerTest(const AutozoomToastControllerTest&) = delete;
  AutozoomToastControllerTest& operator=(const AutozoomToastControllerTest&) =
      delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    auto delegate = std::make_unique<TestDelegate>();
    delegate_ = delegate.get();
    controller_ = std::make_unique<AutozoomToastController>(
        GetPrimaryUnifiedSystemTray(), std::move(delegate));
  }

  void TearDown() override {
    controller_ = nullptr;
    delegate_ = nullptr;
    AshTestBase::TearDown();
  }

  views::Widget* bubble_widget() {
    return controller_->bubble_widget_for_test();
  }

  std::unique_ptr<AutozoomToastController> controller_;
  raw_ptr<TestDelegate, DanglingUntriaged> delegate_;
};

TEST_F(AutozoomToastControllerTest, ShowToastWhenCameraActive) {
  EXPECT_EQ(bubble_widget(), nullptr);

  // No toast when enabling camera when autozoom is disabled.
  delegate_->SetAutozoomControlEnabled(true);
  EXPECT_EQ(bubble_widget(), nullptr);

  // No toast when enabling autozoom when camera is already active.
  delegate_->SetAutozoomEnabled(true);
  EXPECT_EQ(bubble_widget(), nullptr);

  // Toast is shown when autozoom is enabled when camera become active.
  delegate_->SetAutozoomControlEnabled(false);
  delegate_->SetAutozoomControlEnabled(true);
  ASSERT_NE(bubble_widget(), nullptr);
  EXPECT_TRUE(bubble_widget()->IsVisible());
}

}  // namespace ash

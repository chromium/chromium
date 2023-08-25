// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/resize_util.h"

#include <memory>

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "ash/public/cpp/system/scoped_toast_pause.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {
namespace {

constexpr char kTestAppId[] = "123";

class FakeToastManager : public ash::ToastManager {
 public:
  ~FakeToastManager() override = default;

  // ToastManager overrides:
  void Show(ash::ToastData data) override { called_show_ = true; }
  void Cancel(const std::string& id) override { called_cancel_ = true; }
  bool MaybeToggleA11yHighlightOnActiveToastDismissButton(
      const std::string& id) override {
    return false;
  }
  bool MaybeActivateHighlightedDismissButtonOnActiveToast(
      const std::string& id) override {
    return false;
  }
  bool IsRunning(const std::string& id) const override { return false; }
  std::unique_ptr<ash::ScopedToastPause> CreateScopedPause() override {
    return nullptr;
  }
  void Pause() override {}
  void Resume() override {}

  void ResetState() {
    called_show_ = false;
    called_cancel_ = false;
  }

  bool called_show() { return called_show_; }
  bool called_cancel() { return called_cancel_; }

 private:
  bool called_show_{false};
  bool called_cancel_{false};
};

}  // namespace

class ResizeUtilTest : public CompatModeTestBase {
 public:
  // Overridden from test::Test.
  void SetUp() override {
    CompatModeTestBase::SetUp();
    widget_ = CreateArcWidget(std::string(kTestAppId));
  }
  void TearDown() override {
    widget_->CloseNow();
    CompatModeTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
};

// Test that resize phone works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeLockToPhone) {
  widget()->Maximize();

  // Fake a restore state to make sure resizing always results in normal state.
  widget()->GetNativeWindow()->SetProperty(aura::client::kRestoreShowStateKey,
                                           ui::SHOW_STATE_MAXIMIZED);

  // Test the widget is resized.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_TRUE(widget()->IsMaximized());
  ResizeLockToPhone(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
  EXPECT_EQ(PredictCurrentMode(widget()), ResizeCompatMode::kPhone);
}

// Test that resize tablet works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeLockToTablet) {
  widget()->Maximize();

  // Fake a restore state to make sure resizing always results in normal state.
  widget()->GetNativeWindow()->SetProperty(aura::client::kRestoreShowStateKey,
                                           ui::SHOW_STATE_MAXIMIZED);

  // Test the widget is resized.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_TRUE(widget()->IsMaximized());
  ResizeLockToTablet(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
  EXPECT_EQ(PredictCurrentMode(widget()), ResizeCompatMode::kTablet);
}

// Test that resize phone/tablet works properly on small displays.
TEST_F(ResizeUtilTest, TestResizeLockToPhoneTabletOnSmallDisplay) {
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);

  // Set small workarea size.
  constexpr gfx::Size workarea_size(300, 300);
  SetDisplayWorkArea(gfx::Rect(workarea_size));

  // Shrink size according to the workarea size.
  ResizeLockToPhone(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(), workarea_size.width());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().height(),
            workarea_size.height());

  // Don't shrink size so that Android can decide what to do.
  ResizeLockToTablet(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_GE(widget()->GetWindowBoundsInScreen().width(), workarea_size.width());
  EXPECT_GE(widget()->GetWindowBoundsInScreen().height(),
            workarea_size.height());
}

// Test that enabling resizing works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestEnableResizing) {
  FakeToastManager fake_toast_manager;

  // Test the state is NOT changed immediately if the confirmation dialog is
  // needed.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  EnableResizingWithConfirmationIfNeeded(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_NE(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::OFF);
  EXPECT_FALSE(fake_toast_manager.called_cancel());
  EXPECT_FALSE(fake_toast_manager.called_show());

  // Test the state is changed without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EnableResizingWithConfirmationIfNeeded(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_EQ(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::OFF);
  EXPECT_EQ(PredictCurrentMode(widget()), ResizeCompatMode::kResizable);
  EXPECT_TRUE(fake_toast_manager.called_cancel());
  EXPECT_TRUE(fake_toast_manager.called_show());

  // Test the state is not updated redundantly.
  fake_toast_manager.ResetState();
  EnableResizingWithConfirmationIfNeeded(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_FALSE(fake_toast_manager.called_cancel());
  EXPECT_FALSE(fake_toast_manager.called_show());
}

// Test that should show dialog screen dialog caps at a preset limit
TEST_F(ResizeUtilTest, TestShouldShowSplashScreenDialog) {
  // Defines maximum number of showing splash screen per user.
  const int kMaxNumSplashScreen = 2;
  pref_delegate()->SetShowSplashScreenDialogCount(kMaxNumSplashScreen);

  for (int i = 0; i < kMaxNumSplashScreen; i++)
    EXPECT_TRUE(ShouldShowSplashScreenDialog(pref_delegate()));
  EXPECT_FALSE(ShouldShowSplashScreenDialog(pref_delegate()));
}

// Test that an unresizable app is not in resizable mode.
TEST_F(ResizeUtilTest, TestPredictCurrentModeForUnresizable) {
  widget()->widget_delegate()->SetCanResize(false);
  ResizeLockToPhone(widget(), pref_delegate());
  SyncResizeLockPropertyWithMojoState(widget());
  EXPECT_EQ(PredictCurrentMode(widget()), ResizeCompatMode::kPhone);
}

}  // namespace arc

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test_shell_delegate.h"

#include <memory>
#include <string>

#include "ash/accelerators/test_accelerator_prefs_delegate.h"
#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/api/tasks/tasks_delegate.h"
#include "ash/api/tasks/test_tasks_delegate.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/clipboard/test_support/test_clipboard_history_controller_delegate_impl.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/public/cpp/test/test_desk_profiles_delegate.h"
#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/public/cpp/test/test_tab_strip_delegate.h"
#include "ash/scanner/fake_scanner_delegate.h"
#include "ash/system/focus_mode/test/test_focus_mode_delegate.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/test_system_sounds_delegate.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_delegate.h"
#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ash {

TestShellDelegate::TestShellDelegate()
    : url_loader_factory_(
          base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {}

TestShellDelegate::~TestShellDelegate() = default;

bool TestShellDelegate::CanShowWindowForUser(const aura::Window* window) const {
  return true;
}

std::unique_ptr<CaptureModeDelegate>
TestShellDelegate::CreateCaptureModeDelegate() const {
  return std::make_unique<TestCaptureModeDelegate>();
}

std::unique_ptr<ClipboardHistoryControllerDelegate>
TestShellDelegate::CreateClipboardHistoryControllerDelegate() const {
  return std::make_unique<TestClipboardHistoryControllerDelegateImpl>();
}

std::unique_ptr<CoralDelegate> TestShellDelegate::CreateCoralDelegate() const {
  return nullptr;
}

std::unique_ptr<GameDashboardDelegate>
TestShellDelegate::CreateGameDashboardDelegate() const {
  return std::make_unique<TestGameDashboardDelegate>();
}

std::unique_ptr<AcceleratorPrefsDelegate>
TestShellDelegate::CreateAcceleratorPrefsDelegate() const {
  return std::make_unique<TestAcceleratorPrefsDelegate>();
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

std::unique_ptr<BackGestureContextualNudgeDelegate>
TestShellDelegate::CreateBackGestureContextualNudgeDelegate(
    BackGestureContextualNudgeController* controller) {
  return std::make_unique<TestBackGestureContextualNudgeDelegate>(controller);
}

std::unique_ptr<MediaNotificationProvider>
TestShellDelegate::CreateMediaNotificationProvider() {
  return nullptr;
}

std::unique_ptr<NearbyShareDelegate>
TestShellDelegate::CreateNearbyShareDelegate(
    NearbyShareController* controller) const {
  return std::make_unique<TestNearbyShareDelegate>();
}

std::unique_ptr<SavedDeskDelegate> TestShellDelegate::CreateSavedDeskDelegate()
    const {
  return std::make_unique<TestSavedDeskDelegate>();
}

std::unique_ptr<SystemSoundsDelegate>
TestShellDelegate::CreateSystemSoundsDelegate() const {
  return std::make_unique<TestSystemSoundsDelegate>();
}

std::unique_ptr<api::TasksDelegate> TestShellDelegate::CreateTasksDelegate()
    const {
  return std::make_unique<api::TestTasksDelegate>();
}

std::unique_ptr<TabStripDelegate> TestShellDelegate::CreateTabStripDelegate()
    const {
  return std::make_unique<TestTabStripDelegate>();
}

std::unique_ptr<FocusModeDelegate> TestShellDelegate::CreateFocusModeDelegate()
    const {
  return std::make_unique<TestFocusModeDelegate>();
}

std::unique_ptr<UserEducationDelegate>
TestShellDelegate::CreateUserEducationDelegate() const {
  return user_education_delegate_factory_
             ? user_education_delegate_factory_.Run()
             : std::make_unique<testing::NiceMock<MockUserEducationDelegate>>();
}

std::unique_ptr<ScannerDelegate> TestShellDelegate::CreateScannerDelegate()
    const {
  return std::make_unique<FakeScannerDelegate>();
}

scoped_refptr<network::SharedURLLoaderFactory>
TestShellDelegate::GetBrowserProcessUrlLoaderFactory() const {
  return url_loader_factory_;
}

bool TestShellDelegate::CanGoBack(gfx::NativeWindow window) const {
  return can_go_back_;
}

void TestShellDelegate::SetTabScrubberChromeOSEnabled(bool enabled) {
  tab_scrubber_enabled_ = enabled;
}

void TestShellDelegate::ShouldExitFullscreenBeforeLock(
    ShouldExitFullscreenCallback callback) {
  std::move(callback).Run(should_exit_fullscreen_before_lock_);
}

bool TestShellDelegate::ShouldWaitForTouchPressAck(gfx::NativeWindow window) {
  return should_wait_for_touch_ack_;
}

int TestShellDelegate::GetBrowserWebUITabStripHeight() {
  return 0;
}

DeskProfilesDelegate* TestShellDelegate::GetDeskProfilesDelegate() {
  if (!test_desk_profiles_delegate_) {
    test_desk_profiles_delegate_ = std::make_unique<TestDeskProfilesDelegate>();
  }
  return test_desk_profiles_delegate_.get();
}

void TestShellDelegate::OpenMultitaskingSettings() {
  // Opening the settings page will cause a window activation and end overview.
  // Call `EndOverview()` to simulate opening the settings page.
  OverviewController::Get()->EndOverview(OverviewEndAction::kTests);
}

void TestShellDelegate::BindMultiDeviceSetup(
    mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  if (multidevice_setup_binder_)
    multidevice_setup_binder_.Run(std::move(receiver));
}

void TestShellDelegate::BindMultiCaptureService(
    mojo::PendingReceiver<video_capture::mojom::MultiCaptureService> receiver) {
}

void TestShellDelegate::SetCanGoBack(bool can_go_back) {
  can_go_back_ = can_go_back;
}

void TestShellDelegate::SetShouldExitFullscreenBeforeLock(
    bool should_exit_fullscreen_before_lock) {
  should_exit_fullscreen_before_lock_ = should_exit_fullscreen_before_lock;
}

void TestShellDelegate::SetShouldWaitForTouchAck(
    bool should_wait_for_touch_ack) {
  should_wait_for_touch_ack_ = should_wait_for_touch_ack;
}

bool TestShellDelegate::IsSessionRestoreInProgress() const {
  return session_restore_in_progress_;
}

void TestShellDelegate::SetSessionRestoreInProgress(bool in_progress) {
  session_restore_in_progress_ = in_progress;
}

bool TestShellDelegate::IsLoggingRedirectDisabled() const {
  return false;
}

base::FilePath TestShellDelegate::GetPrimaryUserDownloadsFolder() const {
  return base::FilePath();
}

void TestShellDelegate::OpenFeedbackDialog(
    ShellDelegate::FeedbackSource source,
    const std::string& description_template,
    const std::string& category_tag) {
  ++open_feedback_dialog_call_count_;
}

const GURL& TestShellDelegate::GetLastCommittedURLForWindowIfAny(
    aura::Window* window) {
  return last_committed_url_;
}

void TestShellDelegate::SetLastCommittedURLForWindow(const GURL& url) {
  last_committed_url_ = url;
}

version_info::Channel TestShellDelegate::GetChannel() {
  return channel_;
}

std::string TestShellDelegate::GetVersionString() {
  return version_string_;
}

}  // namespace ash

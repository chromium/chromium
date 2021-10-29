// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_SHELL_DELEGATE_H_

#include <memory>

#include "ash/shell_delegate.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class CancelableTaskTracker;
}  // namespace base

namespace ash {

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();

  TestShellDelegate(const TestShellDelegate&) = delete;
  TestShellDelegate& operator=(const TestShellDelegate&) = delete;

  ~TestShellDelegate() override;

  // Allows tests to override the MultiDeviceSetup binding behavior for this
  // TestShellDelegate.
  using MultiDeviceSetupBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup>)>;
  void SetMultiDeviceSetupBinder(MultiDeviceSetupBinder binder) {
    multidevice_setup_binder_ = std::move(binder);
  }

  // Overridden from ShellDelegate:
  bool CanShowWindowForUser(const aura::Window* window) const override;
  std::unique_ptr<CaptureModeDelegate> CreateCaptureModeDelegate()
      const override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<BackGestureContextualNudgeDelegate>
  CreateBackGestureContextualNudgeDelegate(
      BackGestureContextualNudgeController* controller) override;
  bool CanGoBack(gfx::NativeWindow window) const override;
  void SetTabScrubberEnabled(bool enabled) override;
  bool ShouldWaitForTouchPressAck(gfx::NativeWindow window) override;
  int GetBrowserWebUITabStripHeight() override;
  void BindMultiDeviceSetup(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup> receiver)
      override;
  std::unique_ptr<NearbyShareDelegate> CreateNearbyShareDelegate(
      NearbyShareController* controller) const override;
  bool IsSessionRestoreInProgress() const override;
  void SetUpEnvironmentForLockedFullscreen(bool locked) override {}

  void SetCanGoBack(bool can_go_back);
  void SetShouldWaitForTouchAck(bool should_wait_for_touch_ack);
  void SetSessionRestoreInProgress(bool in_progress);
  bool IsLoggingRedirectDisabled() const override;
  base::FilePath GetPrimaryUserDownloadsFolder() const override;
  void OpenFeedbackPageForPersistentDesksBar() override {}
  std::unique_ptr<app_restore::AppLaunchInfo> GetAppLaunchDataForDeskTemplate(
      aura::Window* window) const override;
  void GetFaviconForUrl(const std::string& page_url,
                        int desired_icon_size,
                        favicon_base::FaviconRawBitmapCallback callback,
                        base::CancelableTaskTracker* tracker) const override;
  void GetIconForAppId(
      const std::string& app_id,
      int desired_icon_size,
      base::OnceCallback<void(apps::mojom::IconValuePtr icon_value)> callback)
      const override;
  void LaunchAppsFromTemplate(
      std::unique_ptr<DeskTemplate> desk_template) override;

 private:
  // True if the current top window can go back.
  bool can_go_back_ = true;

  // True if the tab scrubber is enabled.
  bool tab_scrubber_enabled_ = true;

  // True if when performing back gesture on the top window, we should handle
  // the event after the touch ack is received. Please refer to
  // |BackGestureEventHandler::should_wait_for_touch_ack_| for detailed
  // description.
  bool should_wait_for_touch_ack_ = false;

  // True if window browser sessions are restoring.
  bool session_restore_in_progress_ = false;

  MultiDeviceSetupBinder multidevice_setup_binder_;
};

}  // namespace ash

#endif  // ASH_TEST_SHELL_DELEGATE_H_

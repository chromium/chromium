// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace ash {

class UserEducationDelegate;
class WindowState;

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();

  TestShellDelegate(const TestShellDelegate&) = delete;
  TestShellDelegate& operator=(const TestShellDelegate&) = delete;

  ~TestShellDelegate() override;

  // Allows tests to override the MultiDeviceSetup binding behavior for this
  // TestShellDelegate.
  using MultiDeviceSetupBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>)>;
  void SetMultiDeviceSetupBinder(MultiDeviceSetupBinder binder) {
    multidevice_setup_binder_ = std::move(binder);
  }

  // Allows tests to override the `UserEducationDelegate` creation behavior for
  // this `TestShellDelegate`.
  using UserEducationDelegateFactory =
      base::RepeatingCallback<std::unique_ptr<UserEducationDelegate>()>;
  void SetUserEducationDelegateFactory(UserEducationDelegateFactory factory) {
    user_education_delegate_factory_ = std::move(factory);
  }

  // Overridden from ShellDelegate:
  bool CanShowWindowForUser(const aura::Window* window) const override;
  std::unique_ptr<CaptureModeDelegate> CreateCaptureModeDelegate()
      const override;
  std::unique_ptr<GameDashboardDelegate> CreateGameDashboardDelegate()
      const override;
  std::unique_ptr<AcceleratorPrefsDelegate> CreateAcceleratorPrefsDelegate()
      const override;
  std::unique_ptr<GlanceablesDelegate> CreateGlanceablesDelegate(
      GlanceablesController* controller) const override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<BackGestureContextualNudgeDelegate>
  CreateBackGestureContextualNudgeDelegate(
      BackGestureContextualNudgeController* controller) override;
  std::unique_ptr<MediaNotificationProvider> CreateMediaNotificationProvider()
      override;
  std::unique_ptr<NearbyShareDelegate> CreateNearbyShareDelegate(
      NearbyShareController* controller) const override;
  std::unique_ptr<SavedDeskDelegate> CreateSavedDeskDelegate() const override;
  std::unique_ptr<SystemSoundsDelegate> CreateSystemSoundsDelegate()
      const override;
  std::unique_ptr<UserEducationDelegate> CreateUserEducationDelegate()
      const override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetGeolocationUrlLoaderFactory() const override;
  bool CanGoBack(gfx::NativeWindow window) const override;
  void SetTabScrubberChromeOSEnabled(bool enabled) override;
  void ShouldExitFullscreenBeforeLock(
      ShouldExitFullscreenCallback callback) override;
  bool ShouldWaitForTouchPressAck(gfx::NativeWindow window) override;
  int GetBrowserWebUITabStripHeight() override;
  void BindMultiDeviceSetup(
      mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
          receiver) override;
  void BindMultiCaptureService(
      mojo::PendingReceiver<video_capture::mojom::MultiCaptureService> receiver)
      override;
  bool IsSessionRestoreInProgress() const override;
  void SetUpEnvironmentForLockedFullscreen(
      const WindowState& window_state) override {}
  const GURL& GetLastCommittedURLForWindowIfAny(aura::Window* window) override;
  void ForceSkipWarningUserOnClose(
      const std::vector<aura::Window*>& windows) override {}

  void SetCanGoBack(bool can_go_back);
  void SetShouldExitFullscreenBeforeLock(
      bool should_exit_fullscreen_before_lock);
  void SetShouldWaitForTouchAck(bool should_wait_for_touch_ack);
  void SetSessionRestoreInProgress(bool in_progress);
  bool IsLoggingRedirectDisabled() const override;
  base::FilePath GetPrimaryUserDownloadsFolder() const override;
  void OpenFeedbackDialog(FeedbackSource source,
                          const std::string& description_template) override {}
  void SetLastCommittedURLForWindow(const GURL& url);
  version_info::Channel GetChannel() override;
  std::string GetVersionString() override;

  void set_channel(version_info::Channel channel) { channel_ = channel; }

  void set_version_string(const std::string& string) {
    version_string_ = string;
  }

 private:
  // True if the current top window can go back.
  bool can_go_back_ = true;

  // True if the tab scrubber is enabled.
  bool tab_scrubber_enabled_ = true;

  // False if it is allowed by policy to keep fullscreen after unlock.
  bool should_exit_fullscreen_before_lock_ = true;

  // True if when performing back gesture on the top window, we should handle
  // the event after the touch ack is received. Please refer to
  // |BackGestureEventHandler::should_wait_for_touch_ack_| for detailed
  // description.
  bool should_wait_for_touch_ack_ = false;

  // True if window browser sessions are restoring.
  bool session_restore_in_progress_ = false;

  MultiDeviceSetupBinder multidevice_setup_binder_;
  UserEducationDelegateFactory user_education_delegate_factory_;

  GURL last_committed_url_ = GURL::EmptyGURL();

  version_info::Channel channel_ = version_info::Channel::UNKNOWN;

  std::string version_string_;
};

}  // namespace ash

#endif  // ASH_TEST_SHELL_DELEGATE_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_

#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_types.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/views/view_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class Accelerator;
}  // namespace ui

namespace views {
class View;
class WebDialogView;
class Widget;
}  // namespace views

class LoginScreenClientImpl;

namespace ash {
class CaptivePortalDialogDelegate;
class LayoutWidgetDelegateView;
class LoginDisplayHostMojo;
class OobeWebDialogView;

// This class manages the behavior of the Oobe UI dialog.
// And its lifecycle is managed by the widget created in Show().
//   WebDialogView<----delegate_----OobeUIDialogDelegate
//         |
//         |
//         V
//   clientView---->Widget's view hierarchy
class OobeUIDialogDelegate : public ui::WebDialogDelegate,
                             public ChromeKeyboardControllerClient::Observer,
                             public CaptivePortalWindowProxy::Observer,
                             public OobeUI::Observer,
                             public views::ViewObserver,
                             public SystemTrayObserver {
 public:
  explicit OobeUIDialogDelegate(base::WeakPtr<LoginDisplayHostMojo> controller);

  OobeUIDialogDelegate(const OobeUIDialogDelegate&) = delete;
  OobeUIDialogDelegate& operator=(const OobeUIDialogDelegate&) = delete;

  ~OobeUIDialogDelegate() override;

  // Show the dialog widget.
  void Show();

  // Show the dialog widget stretched to full screen.
  void ShowFullScreen();

  // Close the widget, and it will delete this object.
  void Close();

  // Hide the dialog widget, but do not shut it down.
  void Hide();

  // Returns whether the dialog is currently visible.
  bool IsVisible();

  // Update the oobe state of the dialog.
  void SetState(OobeDialogState state);

  // Tell the dialog whether to call FixCaptivePortal next time it is shown.
  void SetShouldDisplayCaptivePortal(bool should_display);

  content::WebContents* GetWebContents();

  OobeUI* GetOobeUI() const;
  gfx::NativeWindow GetNativeWindow() const;

  views::Widget* GetWebDialogWidget() const;

  views::View* GetWebDialogView();

  CaptivePortalDialogDelegate* captive_portal_delegate_for_test() {
    return captive_portal_delegate_.get();
  }

 private:
  // ui::WebDialogDelegate:
  // NOTE: This function starts cleanup sequence that would call FinishCleanup
  // and delete this object in the end.
  void OnDialogClosed(const std::string& json_retval) override;
  std::vector<ui::Accelerator> GetAccelerators() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // CaptivePortalWindowProxy::Observer:
  void OnBeforeCaptivePortalShown() override;
  void OnAfterCaptivePortalHidden() override;

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;

  base::WeakPtr<LoginDisplayHostMojo> controller_;

  base::WeakPtr<CaptivePortalDialogDelegate> captive_portal_delegate_;

  // Root widget. It is assumed that widget is placed as a full-screen inside
  // LockContainer.
  raw_ptr<views::Widget, ExperimentalAsh> widget_ = nullptr;
  // Reference to view owned by widget_.
  raw_ptr<LayoutWidgetDelegateView, ExperimentalAsh> layout_view_ = nullptr;
  // Reference to dialog view stored in widget_.
  raw_ptr<OobeWebDialogView, ExperimentalAsh> dialog_view_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
  base::ScopedObservation<ChromeKeyboardControllerClient,
                          ChromeKeyboardControllerClient::Observer>
      keyboard_observer_{this};
  base::ScopedObservation<CaptivePortalWindowProxy,
                          CaptivePortalWindowProxy::Observer>
      captive_portal_observer_{this};
  base::ScopedObservation<OobeUI, OobeUI::Observer> oobe_ui_observer_{this};

  base::ScopedObservation<LoginScreenClientImpl, SystemTrayObserver>
      scoped_system_tray_observer_{this};

  std::map<ui::Accelerator, LoginAcceleratorAction> accel_map_;
  OobeDialogState state_ = OobeDialogState::HIDDEN;

  // Whether the captive portal screen should be shown the next time the Gaia
  // dialog is opened.
  bool should_display_captive_portal_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_

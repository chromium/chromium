// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_

#include <string>

#include "ash/public/cpp/login_types.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/views/view_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class WebContents;
}

namespace ui {
class Accelerator;
}

namespace views {
class View;
class WebDialogView;
class Widget;
}  // namespace views

namespace chromeos {

class CaptivePortalDialogDelegate;
class LayoutWidgetDelegateView;
class LoginDisplayHostMojo;
class OobeUI;
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
                             public views::ViewObserver {
 public:
  explicit OobeUIDialogDelegate(base::WeakPtr<LoginDisplayHostMojo> controller);
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
  void SetState(ash::OobeDialogState state);

  // Tell the dialog whether to call FixCaptivePortal next time it is shown.
  void SetShouldDisplayCaptivePortal(bool should_display);

  content::WebContents* GetWebContents();

  OobeUI* GetOobeUI() const;
  gfx::NativeWindow GetNativeWindow() const;

 private:
  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool CanResizeDialog() const override;
  std::string GetDialogArgs() const override;
  // NOTE: This function starts cleanup sequence that would call FinishCleanup
  // and delete this object in the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  std::vector<ui::Accelerator> GetAccelerators() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // CaptivePortalWindowProxy::Observer:
  void OnBeforeCaptivePortalShown() override;
  void OnAfterCaptivePortalHidden() override;

  base::WeakPtr<LoginDisplayHostMojo> controller_;

  base::WeakPtr<CaptivePortalDialogDelegate> captive_portal_delegate_;

  // Root widget. It is assumed that widget is placed as a full-screen inside
  // LockContainer.
  views::Widget* widget_ = nullptr;
  // Reference to view owned by widget_.
  LayoutWidgetDelegateView* layout_view_ = nullptr;
  // Reference to dialog view stored in widget_.
  OobeWebDialogView* dialog_view_ = nullptr;

  ScopedObserver<ChromeKeyboardControllerClient,
                 ChromeKeyboardControllerClient::Observer>
      keyboard_observer_{this};
  ScopedObserver<CaptivePortalWindowProxy, CaptivePortalWindowProxy::Observer>
      captive_portal_observer_{this};

  std::map<ui::Accelerator, std::string> accel_map_;
  ash::OobeDialogState state_ = ash::OobeDialogState::HIDDEN;

  // Whether the captive portal screen should be shown the next time the Gaia
  // dialog is opened.
  bool should_display_captive_portal_ = false;

  DISALLOW_COPY_AND_ASSIGN(OobeUIDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_VIEW_H_

#include <map>
#include <string>

#include "ash/public/cpp/system_tray_focus_observer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace content {
class WebUI;
}

namespace views {
class View;
class WebView;
class Widget;
}  // namespace views

namespace chromeos {

class OobeUI;

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a WebView.
class WebUILoginView : public views::View,
                       public ChromeKeyboardControllerClient::Observer,
                       public content::WebContentsDelegate,
                       public content::NotificationObserver,
                       public ChromeWebModalDialogManagerDelegate,
                       public web_modal::WebContentsModalDialogHost,
                       public ash::SystemTrayFocusObserver {
 public:
  struct WebViewSettings {
    // If true, this will check for and consume a preloaded views::WebView
    // instance.
    bool check_for_preload = false;

    // Title of the web contents. This will be shown in the task manager. If
    // empty, the default webview title will be used.
    base::string16 web_view_title;
  };

  // Internal class name.
  static const char kViewClassName[];

  explicit WebUILoginView(const WebViewSettings& settings);
  ~WebUILoginView() override;

  // Initializes the webui login view.
  virtual void Init();

  // Overridden from views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  const char* GetClassName() const override;
  void RequestFocus() override;

  // Overridden from ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Overridden from web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // Gets the native window from the view widget.
  gfx::NativeWindow GetNativeWindow() const;

  // Loads given page. Should be called after Init() has been called.
  void LoadURL(const GURL& url);

  // Returns current WebUI.
  content::WebUI* GetWebUI();

  // Returns current WebContents.
  content::WebContents* GetWebContents();

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI();

  // Called when WebUI is being shown after being initilized hidden.
  void OnPostponedShow();

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Sets whether UI should be enabled.
  void SetUIEnabled(bool enabled);

  void set_is_hidden(bool hidden) { is_hidden_ = hidden; }

  bool webui_visible() const { return webui_visible_; }

  // Let suppress emission of this signal.
  void set_should_emit_login_prompt_visible(bool emit) {
    should_emit_login_prompt_visible_ = emit;
  }

 protected:
  static void InitializeWebView(views::WebView* web_view,
                                const base::string16& title);

  // Overridden from views::View:
  void Layout() override;
  void ChildPreferredSizeChanged(View* child) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // Overridden from content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  views::WebView* web_view();

 private:
  // Map type for the accelerator-to-identifier map.
  typedef std::map<ui::Accelerator, std::string> AccelMap;

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // Overridden from content::WebContentsDelegate.
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

  // Overridden from ash::SystemTrayFocusObserver.
  void OnFocusLeavingSystemTray(bool reverse) override;

  // Attempts to move focus to system tray. Returns whether the attempt was
  // successful (it might fail if the system tray is not visible).
  bool MoveFocusToSystemTray(bool reverse);

  // Performs series of actions when login prompt is considered
  // to be ready and visible.
  // 1. Emits LoginPromptVisible signal if needed
  // 2. Notifies OOBE/sign classes.
  void OnLoginPromptVisible();

  content::NotificationRegistrar registrar_;

  // WebView configuration options.
  const WebViewSettings settings_;

  // WebView for rendering a webpage as a webui login.
  std::unique_ptr<views::WebView> webui_login_;

  // Converts keyboard events on the WebContents to accelerators.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Maps installed accelerators to OOBE webui accelerator identifiers.
  AccelMap accel_map_;

  // True when WebUI is being initialized hidden.
  bool is_hidden_ = false;

  // True when the WebUI has finished initializing and is visible.
  bool webui_visible_ = false;

  // Should we emit the login-prompt-visible signal when the login page is
  // displayed?
  bool should_emit_login_prompt_visible_ = true;

  // True to forward keyboard event.
  bool forward_keyboard_event_ = true;

  bool observing_system_tray_focus_ = false;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_VIEW_H_

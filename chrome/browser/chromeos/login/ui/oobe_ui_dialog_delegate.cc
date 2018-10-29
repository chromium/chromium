// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/oobe_ui_dialog_delegate.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr char kGaiaURL[] = "chrome://oobe/gaia-signin";
constexpr int kGaiaDialogHeight = 640;
constexpr int kGaiaDialogWidth = 768;
constexpr char kAppLaunchBailout[] = "app_launch_bailout";
constexpr char kCancel[] = "cancel";

}  // namespace

class OobeWebDialogView : public views::WebDialogView {
 public:
  OobeWebDialogView(content::BrowserContext* context,
                    ui::WebDialogDelegate* delegate,
                    WebContentsHandler* handler)
      : views::WebDialogView(context, delegate, handler) {}

  // views::WebDialogView:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    // This is required for accessing the camera for SAML logins.
    MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
        web_contents, request, std::move(callback), nullptr /* extension */);
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
  }
};

class CaptivePortalDialogDelegate
    : public ui::WebDialogDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  explicit CaptivePortalDialogDelegate(views::WebDialogView* host_dialog_view)
      : host_view_(host_dialog_view),
        web_contents_(host_dialog_view->web_contents()) {
    view_ = new views::WebDialogView(ProfileHelper::GetSigninProfile(), this,
                                     new ChromeWebContentsHandler);
    view_->SetVisible(false);

    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = view_;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    ash_util::SetupWidgetInitParamsForContainer(
        &params, ash::kShellWindowId_LockSystemModalContainer);

    widget_ = new views::Widget;
    widget_->Init(params);
    widget_->SetBounds(display::Screen::GetScreen()
                           ->GetDisplayNearestWindow(widget_->GetNativeWindow())
                           .work_area());
    widget_->SetOpacity(0);

    // Set this as the web modal delegate so that captive portal dialog can
    // appear.
    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        web_contents_);
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
        ->SetDelegate(this);
  }

  void Show() { widget_->Show(); }

  void Hide() { widget_->Hide(); }

  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override {
    return this;
  }

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override {
    if (widget_->IsVisible())
      return widget_->GetNativeWindow();
    else
      return host_view_->GetWidget()->GetNativeWindow();
  }

  gfx::Point GetDialogPosition(const gfx::Size& size) override {
    // Center the dialog in the screen.
    gfx::Size host_size = GetHostView()->GetBoundsInScreen().size();
    return gfx::Point(host_size.width() / 2 - size.width() / 2,
                      host_size.height() / 2 - size.height() / 2);
  }

  gfx::Size GetMaximumDialogSize() override {
    return display::Screen::GetScreen()
        ->GetDisplayNearestWindow(widget_->GetNativeWindow())
        .work_area()
        .size();
  }

  void AddObserver(web_modal::ModalDialogHostObserver* observer) override {}

  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override {}

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override {
    return ui::ModalType::MODAL_TYPE_SYSTEM;
  }

  base::string16 GetDialogTitle() const override { return base::string16(); }

  GURL GetDialogContentURL() const override { return GURL(); }

  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override {}

  void GetDialogSize(gfx::Size* size) const override {
    *size = display::Screen::GetScreen()
                ->GetDisplayNearestWindow(widget_->GetNativeWindow())
                .work_area()
                .size();
  }

  std::string GetDialogArgs() const override { return std::string(); }

  void OnDialogClosed(const std::string& json_retval) override { delete this; }

  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override {
    *out_close_dialog = false;
  }

  bool ShouldShowDialogTitle() const override { return false; }

 private:
  views::Widget* widget_ = nullptr;
  views::WebDialogView* view_ = nullptr;
  views::WebDialogView* host_view_ = nullptr;
  content::WebContents* web_contents_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalDialogDelegate);
};

OobeUIDialogDelegate::OobeUIDialogDelegate(
    base::WeakPtr<LoginDisplayHostMojo> controller)
    : controller_(controller),
      size_(gfx::Size(kGaiaDialogWidth, kGaiaDialogHeight)) {
  display_observer_.Add(display::Screen::GetScreen());
  tablet_mode_observer_.Add(TabletModeClient::Get());
  keyboard_observer_.Add(ChromeKeyboardControllerClient::Get());

  accel_map_[ui::Accelerator(
      ui::VKEY_S, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] = kAppLaunchBailout;
  accel_map_[ui::Accelerator(ui::VKEY_ESCAPE, 0)] = kCancel;

  DCHECK(!dialog_view_ && !dialog_widget_);
  // Life cycle of |dialog_view_| is managed by the widget:
  // Widget owns a root view which has |dialog_view_| as its child view.
  // Before the widget is destroyed, it will clean up the view hierarchy
  // starting from root view.
  dialog_view_ = new OobeWebDialogView(ProfileHelper::GetSigninProfile(), this,
                                       new ChromeWebContentsHandler);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = dialog_view_;
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_LockSystemModalContainer);

  dialog_widget_ = new views::Widget;
  dialog_widget_->Init(params);

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      dialog_view_->web_contents());

  dialog_view_->web_contents()->SetDelegate(this);

  captive_portal_delegate_ = new CaptivePortalDialogDelegate(dialog_view_);

  GetOobeUI()->GetErrorScreen()->MaybeInitCaptivePortalWindowProxy(
      dialog_view_->web_contents());
  captive_portal_observer_.Add(
      GetOobeUI()->GetErrorScreen()->captive_portal_window_proxy());
}

OobeUIDialogDelegate::~OobeUIDialogDelegate() {
  if (controller_)
    controller_->OnDialogDestroyed(this);
}

content::WebContents* OobeUIDialogDelegate::GetWebContents() {
  return dialog_view_->web_contents();
}

bool OobeUIDialogDelegate::IsVisible() {
  return dialog_widget_->IsVisible();
}

void OobeUIDialogDelegate::SetShouldDisplayCaptivePortal(bool should_display) {
  should_display_captive_portal_ = should_display;
}

void OobeUIDialogDelegate::Show() {
  dialog_widget_->Show();
  SetState(ash::mojom::OobeDialogState::GAIA_SIGNIN);

  if (should_display_captive_portal_)
    GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
}

void OobeUIDialogDelegate::ShowFullScreen() {
  const gfx::Size& size =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dialog_widget_->GetNativeWindow())
          .size();
  UpdateSizeAndPosition(size.width(), size.height());
  Show();
  showing_fullscreen_ = true;
}

void OobeUIDialogDelegate::Hide() {
  if (!dialog_widget_)
    return;
  dialog_widget_->Hide();
  SetState(ash::mojom::OobeDialogState::HIDDEN);
}

void OobeUIDialogDelegate::Close() {
  if (!dialog_widget_)
    return;
  // We do not call LoginScreen::NotifyOobeDialogVisibility here, because this
  // would cause the LoginShelfView to update its button visibility even though
  // the login screen is about to be destroyed. See http://crbug/836172
  dialog_widget_->Close();
}

void OobeUIDialogDelegate::SetState(ash::mojom::OobeDialogState state) {
  if (!dialog_widget_ || state_ == state)
    return;

  // Gaia WebUI is preloaded, so it's possible that WebUI send certain state
  // while the widget is not visible. Defer the state update until Show().
  if (!dialog_widget_->IsVisible() &&
      state != ash::mojom::OobeDialogState::HIDDEN) {
    return;
  }

  state_ = state;
  LoginScreenClient::Get()->login_screen()->NotifyOobeDialogState(state_);
}

void OobeUIDialogDelegate::UpdateSizeAndPosition(int width, int height) {
  size_.SetSize(width, height);
  if (!dialog_widget_)
    return;

  // display_manager.js sends the width and height of the content in
  // updateScreenSize() when we show the app launch splash. Without this if
  // statement, the dialog would be resized to just fit the content rather than
  // show fullscreen as expected.
  if (showing_fullscreen_)
    return;

  gfx::Rect display_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dialog_widget_->GetNativeWindow())
          .work_area();

  // Place the dialog in the center of the screen.
  const gfx::Rect bounds(
      display_rect.x() + (display_rect.width() - size_.width()) / 2,
      display_rect.y() + (display_rect.height() - size_.height()) / 2,
      size_.width(), size_.height());
  dialog_widget_->SetBounds(bounds);
}

OobeUI* OobeUIDialogDelegate::GetOobeUI() const {
  if (dialog_view_) {
    content::WebUI* webui = dialog_view_->web_contents()->GetWebUI();
    if (webui)
      return static_cast<OobeUI*>(webui->GetController());
  }
  return nullptr;
}

gfx::NativeWindow OobeUIDialogDelegate::GetNativeWindow() const {
  return dialog_widget_ ? dialog_widget_->GetNativeWindow() : nullptr;
}

bool OobeUIDialogDelegate::TakeFocus(content::WebContents* source,
                                     bool reverse) {
  LoginScreenClient::Get()->login_screen()->FocusLoginShelf(reverse);
  return true;
}

bool OobeUIDialogDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, dialog_widget_->GetFocusManager());
}

void OobeUIDialogDelegate::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!dialog_widget_)
    return;

  const display::Display this_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          dialog_widget_->GetNativeWindow());
  if (this_display.id() == display.id())
    UpdateSizeAndPosition(size_.width(), size_.height());
}

void OobeUIDialogDelegate::OnTabletModeToggled(bool enabled) {
  if (!dialog_widget_)
    return;

  UpdateSizeAndPosition(size_.width(), size_.height());
}

ui::ModalType OobeUIDialogDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 OobeUIDialogDelegate::GetDialogTitle() const {
  return base::string16();
}

GURL OobeUIDialogDelegate::GetDialogContentURL() const {
  return GURL(kGaiaURL);
}

void OobeUIDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void OobeUIDialogDelegate::GetDialogSize(gfx::Size* size) const {
  *size = size_;
}

bool OobeUIDialogDelegate::CanResizeDialog() const {
  return false;
}

std::string OobeUIDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void OobeUIDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void OobeUIDialogDelegate::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool OobeUIDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool OobeUIDialogDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}

std::vector<ui::Accelerator> OobeUIDialogDelegate::GetAccelerators() {
  // TODO(crbug.com/809648): Adding necessary accelerators.
  std::vector<ui::Accelerator> output;

  for (const auto& pair : accel_map_)
    output.push_back(pair.first);

  return output;
}

bool OobeUIDialogDelegate::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  auto entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end())
    return false;

  GetOobeUI()->ForwardAccelerator(entry->second);
  return true;
}

void OobeUIDialogDelegate::OnKeyboardVisibilityChanged(bool visible) {
  if (!dialog_widget_)
    return;

  UpdateSizeAndPosition(size_.width(), size_.height());
}

void OobeUIDialogDelegate::OnBeforeCaptivePortalShown() {
  should_display_captive_portal_ = false;

  captive_portal_delegate_->Show();
}

void OobeUIDialogDelegate::OnAfterCaptivePortalHidden() {
  // If the captive portal state went from hidden -> visible -> back to hidden
  // while the OOBE dialog was not shown, we should not attempt to load the
  // captive portal next time the OOBE dialog pops up.
  should_display_captive_portal_ = false;

  captive_portal_delegate_->Hide();
}

}  // namespace chromeos

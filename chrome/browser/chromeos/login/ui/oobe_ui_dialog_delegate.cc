// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/oobe_ui_dialog_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr char kGaiaURL[] = "chrome://oobe/gaia-signin";

CoreOobeView::DialogPaddingMode ConvertDialogPaddingMode(
    OobeDialogPaddingMode padding) {
  switch (padding) {
    case OobeDialogPaddingMode::PADDING_AUTO:
      return CoreOobeView::DialogPaddingMode::MODE_AUTO;
    case OobeDialogPaddingMode::PADDING_WIDE:
      return CoreOobeView::DialogPaddingMode::MODE_WIDE;
    case OobeDialogPaddingMode::PADDING_NARROW:
      return CoreOobeView::DialogPaddingMode::MODE_NARROW;
  }
}

}  // namespace

class OobeWebDialogView : public views::WebDialogView {
 public:
  OobeWebDialogView(content::BrowserContext* context,
                    ui::WebDialogDelegate* delegate,
                    std::unique_ptr<WebContentsHandler> handler)
      : views::WebDialogView(context, delegate, std::move(handler)) {}

  // content::WebContentsDelegate:
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
                                  blink::mojom::MediaStreamType type) override {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
  }

  bool TakeFocus(content::WebContents* source, bool reverse) override {
    ash::LoginScreen::Get()->FocusLoginShelf(reverse);
    return true;
  }

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

 private:
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(OobeWebDialogView);
};

// View that controls size of OobeUIDialog.
// Dialog can be shown as a full-screen (in this case it will fit whole screen)
// except for Virtual Keyboard or as a window.
// For both dimensions we try to center dialog so that it has size no more than
// kGaiaDialogMaxSize with margins no less than kGaiaDialogMinMargin on each
// side.
// If available height is too small (usually due to virtual keyboard),
// and calculations above would give total dialog height lesser than
// kGaiaDialogMinHeight, then margins will be reduced to accommodate minimum
// height.
// When no virtual keyboard is displayed, ash shelf height should be excluded
// from space available for calculations.
// Host size accounts for virtual keyboard presence, but not for shelf.
// It is assumed that host view is always a full-screen view on a primary
// display.
class LayoutWidgetDelegateView : public views::WidgetDelegateView {
 public:
  LayoutWidgetDelegateView(OobeUIDialogDelegate* dialog_delegate,
                           OobeWebDialogView* oobe_view)
      : dialog_delegate_(dialog_delegate), oobe_view_(oobe_view) {
    SetFocusTraversesOut(true);
    AddChildView(oobe_view_);
  }

  ~LayoutWidgetDelegateView() override { delete dialog_delegate_; }

  void SetFullscreen(bool value) {
    if (fullscreen_ == value)
      return;
    fullscreen_ = value;
    Layout();
  }

  void SetHasShelf(bool value) {
    has_shelf_ = value;
    Layout();
  }

  OobeDialogPaddingMode padding() { return padding_; }

  // views::WidgetDelegateView:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_WINDOW; }

  void Layout() override {
    if (fullscreen_) {
      for (views::View* child : children()) {
        child->SetBoundsRect(GetContentsBounds());
      }
      padding_ = OobeDialogPaddingMode::PADDING_AUTO;
      return;
    }

    gfx::Rect bounds;
    const int shelf_height =
        has_shelf_ ? ash::ShelfConfig::Get()->shelf_size() : 0;
    CalculateOobeDialogBounds(GetContentsBounds(), shelf_height, &bounds,
                              &padding_);

    for (views::View* child : children()) {
      child->SetBoundsRect(bounds);
    }
  }

  View* GetInitiallyFocusedView() override { return oobe_view_; }

 private:
  OobeUIDialogDelegate* dialog_delegate_ = nullptr;  // Owned by us.
  OobeWebDialogView* oobe_view_ = nullptr;  // Owned by views hierarchy.

  // Indicates whether Oobe web view should fully occupy the hosting widget.
  bool fullscreen_ = false;
  // Indicates if ash shelf is displayed (and should be excluded from available
  // space).
  bool has_shelf_ = true;

  // Tracks dialog margins after last size calculations.
  OobeDialogPaddingMode padding_ = OobeDialogPaddingMode::PADDING_AUTO;

  DISALLOW_COPY_AND_ASSIGN(LayoutWidgetDelegateView);
};

class CaptivePortalDialogDelegate
    : public ui::WebDialogDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  explicit CaptivePortalDialogDelegate(views::WebDialogView* host_dialog_view)
      : host_view_(host_dialog_view),
        web_contents_(host_dialog_view->web_contents()) {
    view_ =
        new views::WebDialogView(ProfileHelper::GetSigninProfile(), this,
                                 std::make_unique<ChromeWebContentsHandler>());
    view_->SetVisible(false);

    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = view_;
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    ash_util::SetupWidgetInitParamsForContainer(
        &params, ash::kShellWindowId_LockSystemModalContainer);

    widget_ = new views::Widget;
    widget_->Init(std::move(params));
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

  void Close() { widget_->Close(); }

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

  base::WeakPtr<CaptivePortalDialogDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  views::Widget* widget_ = nullptr;
  views::WebDialogView* view_ = nullptr;
  views::WebDialogView* host_view_ = nullptr;
  content::WebContents* web_contents_ = nullptr;

  base::WeakPtrFactory<CaptivePortalDialogDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalDialogDelegate);
};

OobeUIDialogDelegate::OobeUIDialogDelegate(
    base::WeakPtr<LoginDisplayHostMojo> controller)
    : controller_(controller) {
  set_can_resize(false);
  keyboard_observer_.Add(ChromeKeyboardControllerClient::Get());

  for (size_t i = 0; i < ash::kLoginAcceleratorDataLength; ++i) {
    if (ash::kLoginAcceleratorData[i].global)
      continue;
    if (!(ash::kLoginAcceleratorData[i].scope &
          (ash::kScopeLogin | ash::kScopeLock))) {
      continue;
    }

    accel_map_[ui::Accelerator(ash::kLoginAcceleratorData[i].keycode,
                               ash::kLoginAcceleratorData[i].modifiers)] =
        ash::kLoginAcceleratorData[i].action;
  }

  DCHECK(!dialog_view_ && !widget_);
  // Life cycle of |dialog_view_| is managed by the widget:
  // Widget owns a root view which has |dialog_view_| as its child view.
  // Before the widget is destroyed, it will clean up the view hierarchy
  // starting from root view.
  dialog_view_ =
      new OobeWebDialogView(ProfileHelper::GetSigninProfile(), this,
                            std::make_unique<ChromeWebContentsHandler>());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_LockScreenContainer);
  layout_view_ = new LayoutWidgetDelegateView(this, dialog_view_);
  params.delegate = layout_view_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.show_state = ui::SHOW_STATE_FULLSCREEN;

  widget_ = new views::Widget();
  widget_->Init(std::move(params));

  layout_view_->SetHasShelf(
      !ChromeKeyboardControllerClient::Get()->is_keyboard_visible());

  dialog_view_->AddObserver(this);

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      dialog_view_->web_contents());

  captive_portal_delegate_ =
      (new CaptivePortalDialogDelegate(dialog_view_))->GetWeakPtr();

  GetOobeUI()->GetErrorScreen()->MaybeInitCaptivePortalWindowProxy(
      dialog_view_->web_contents());
  captive_portal_observer_.Add(
      GetOobeUI()->GetErrorScreen()->captive_portal_window_proxy());
}

OobeUIDialogDelegate::~OobeUIDialogDelegate() {
  dialog_view_->RemoveObserver(this);
  if (captive_portal_delegate_)
    captive_portal_delegate_->Close();
  if (controller_)
    controller_->OnDialogDestroyed(this);
}

content::WebContents* OobeUIDialogDelegate::GetWebContents() {
  return dialog_view_->web_contents();
}

bool OobeUIDialogDelegate::IsVisible() {
  return widget_->IsVisible();
}

void OobeUIDialogDelegate::SetShouldDisplayCaptivePortal(bool should_display) {
  should_display_captive_portal_ = should_display;
}

void OobeUIDialogDelegate::Show() {
  widget_->Show();
  if (state_ == ash::OobeDialogState::HIDDEN) {
    SetState(ash::OobeDialogState::GAIA_SIGNIN);
  } else {
    ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(state_);
  }

  if (should_display_captive_portal_)
    GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
}

void OobeUIDialogDelegate::ShowFullScreen() {
  layout_view_->SetFullscreen(true);
  Show();
}

void OobeUIDialogDelegate::Hide() {
  if (!widget_)
    return;
  widget_->Hide();
  SetState(ash::OobeDialogState::HIDDEN);
}

void OobeUIDialogDelegate::Close() {
  if (!widget_)
    return;
  // We do not call LoginScreen::NotifyOobeDialogVisibility here, because this
  // would cause the LoginShelfView to update its button visibility even though
  // the login screen is about to be destroyed. See http://crbug/836172
  widget_->Close();
}

void OobeUIDialogDelegate::SetState(ash::OobeDialogState state) {
  if (!widget_ || state_ == state)
    return;

  state_ = state;

  // Gaia WebUI is preloaded, so it's possible for WebUI to send state updates
  // while the widget is not visible. Defer the state update until Show().
  if (!widget_->IsVisible() && state_ != ash::OobeDialogState::HIDDEN)
    return;

  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(state_);
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
  return widget_ ? widget_->GetNativeWindow() : nullptr;
}

ui::ModalType OobeUIDialogDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
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
  // Dialog will be resized externally by LayoutWidgetDelegateView.
}

std::string OobeUIDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void OobeUIDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  widget_->Close();
}

void OobeUIDialogDelegate::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool OobeUIDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool OobeUIDialogDelegate::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
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
  if (controller_)
    return controller_->HandleAccelerator(entry->second);
  return false;
}

void OobeUIDialogDelegate::OnViewBoundsChanged(views::View* observed_view) {
  if (!widget_)
    return;
  GetOobeUI()->GetCoreOobeView()->SetDialogPaddingMode(
      ConvertDialogPaddingMode(layout_view_->padding()));
}

void OobeUIDialogDelegate::OnKeyboardVisibilityChanged(bool visible) {
  if (!widget_)
    return;
  layout_view_->SetHasShelf(!visible);
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

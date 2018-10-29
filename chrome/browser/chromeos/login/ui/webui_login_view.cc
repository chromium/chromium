// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/webui_login_view.h"

#include <memory>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/login_display_webui.h"
#include "chrome/browser/chromeos/login/ui/preloaded_web_view.h"
#include "chrome/browser/chromeos/login/ui/preloaded_web_view_factory.h"
#include "chrome/browser/chromeos/login/ui/web_contents_forced_title.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/view_type_utils.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using chromeos::AutoEnrollmentController;
using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace {

// These strings must be kept in sync with handleAccelerator()
// in display_manager.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnableDebugging[] = "debugging";
const char kAccelNameEnrollment[] = "enrollment";
const char kAccelNameKioskEnable[] = "kiosk_enable";
const char kAccelNameVersion[] = "version";
const char kAccelNameReset[] = "reset";
const char kAccelNameDeviceRequisition[] = "device_requisition";
const char kAccelNameDeviceRequisitionRemora[] = "device_requisition_remora";
const char kAccelNameDeviceRequisitionShark[] = "device_requisition_shark";
const char kAccelNameAppLaunchBailout[] = "app_launch_bailout";
const char kAccelNameAppLaunchNetworkConfig[] = "app_launch_network_config";
const char kAccelNameBootstrappingSlave[] = "bootstrapping_slave";
const char kAccelNameDemoMode[] = "demo_mode";
const char kAccelSendFeedback[] = "send_feedback";

// A class to change arrow key traversal behavior when it's alive.
class ScopedArrowKeyTraversal {
 public:
  explicit ScopedArrowKeyTraversal(bool new_arrow_key_tranversal_enabled)
      : previous_arrow_key_traversal_enabled_(
            views::FocusManager::arrow_key_traversal_enabled()) {
    views::FocusManager::set_arrow_key_traversal_enabled(
        new_arrow_key_tranversal_enabled);
  }
  ~ScopedArrowKeyTraversal() {
    views::FocusManager::set_arrow_key_traversal_enabled(
        previous_arrow_key_traversal_enabled_);
  }

 private:
  const bool previous_arrow_key_traversal_enabled_;
  DISALLOW_COPY_AND_ASSIGN(ScopedArrowKeyTraversal);
};

}  // namespace

namespace chromeos {

// static
const char WebUILoginView::kViewClassName[] =
    "browser/chromeos/login/WebUILoginView";

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView(const WebViewSettings& settings)
    : settings_(settings) {
  ChromeKeyboardControllerClient::Get()->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                 content::NotificationService::AllSources());

  accel_map_[ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)] = kAccelNameCancel;
  accel_map_[ui::Accelerator(ui::VKEY_E,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameEnrollment;
  if (KioskAppManager::IsConsumerKioskEnabled()) {
    accel_map_[ui::Accelerator(ui::VKEY_K,
                               ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
        kAccelNameKioskEnable;
  }
  accel_map_[ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)] =
      kAccelNameVersion;
  accel_map_[ui::Accelerator(
      ui::VKEY_R, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameReset;
  accel_map_[ui::Accelerator(ui::VKEY_X,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameEnableDebugging;

  accel_map_[ui::Accelerator(
      ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameDeviceRequisition;
  accel_map_[
      ui::Accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameDeviceRequisitionRemora;
  accel_map_[
      ui::Accelerator(ui::VKEY_H,
          ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameDeviceRequisitionShark;

  accel_map_[ui::Accelerator(ui::VKEY_S,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameAppLaunchBailout;

  accel_map_[ui::Accelerator(ui::VKEY_N,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameAppLaunchNetworkConfig;

  accel_map_[ui::Accelerator(
      ui::VKEY_S, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameBootstrappingSlave;

  accel_map_[ui::Accelerator(
      ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] = kAccelNameDemoMode;

  accel_map_[ui::Accelerator(ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)] =
      kAccelSendFeedback;

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i) {
    if (!features::IsMultiProcessMash()) {
      // To make reset accelerator work while system tray is open, register it
      // at accelerator controller.
      ash::Shell::Get()->accelerator_controller()->Register({i->first}, this);
    } else {
      // TODO(crbug.com/782072): In mash, accelerators are not available if
      // system tray is open.
      NOTIMPLEMENTED();
      AddAccelerator(i->first);
    }
  }

  if (!features::IsMultiProcessMash())
    ash::Shell::Get()->system_tray_notifier()->AddSystemTrayFocusObserver(this);
}

WebUILoginView::~WebUILoginView() {
  for (auto& observer : observer_list_)
    observer.OnHostDestroying();

  if (!features::IsMultiProcessMash()) {
    ash::Shell::Get()->system_tray_notifier()->RemoveSystemTrayFocusObserver(
        this);
    ash::Shell::Get()->accelerator_controller()->UnregisterAll(this);
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  // Clear any delegates we have set on the WebView.
  WebContents* web_contents = web_view()->GetWebContents();
  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(nullptr);
  web_contents->SetDelegate(nullptr);
}

// static
void WebUILoginView::InitializeWebView(views::WebView* web_view,
                                       const base::string16& title) {
  WebContents* web_contents = web_view->GetWebContents();

  if (!title.empty())
    WebContentsForcedTitle::CreateForWebContentsWithTitle(web_contents, title);

  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents, SK_ColorTRANSPARENT);

  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from a captive portal window
  // that may be opened on top of this UI.
  SessionTabHelper::CreateForWebContents(web_contents);

  // Create the password manager that is needed for the proxy.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));

  // LoginHandlerViews uses a constrained window for the password manager view.
  WebContentsModalDialogManager::CreateForWebContents(web_contents);

  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  content::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, ProfileHelper::GetSigninProfile());
}

void WebUILoginView::Init() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();

  if (settings_.check_for_preload) {
    PreloadedWebView* preloaded_web_view =
        PreloadedWebViewFactory::GetForProfile(signin_profile);
    // webui_login_ may still be null after this call if there is no preloaded
    // instance.
    webui_login_ = preloaded_web_view->TryTake();
    is_reusing_webview_ = true;
  }

  if (!webui_login_) {
    webui_login_ = std::make_unique<views::WebView>(signin_profile);
    webui_login_->set_owned_by_client();
    is_reusing_webview_ = false;
  }

  WebContents* web_contents = web_view()->GetWebContents();
  if (!is_reusing_webview_)
    InitializeWebView(web_view(), settings_.web_view_title);

  web_view()->set_allow_accelerators(true);
  AddChildView(web_view());

  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
  web_contents->SetDelegate(this);
}

const char* WebUILoginView::GetClassName() const {
  return kViewClassName;
}

void WebUILoginView::RequestFocus() {
  web_view()->RequestFocus();
}

web_modal::WebContentsModalDialogHost*
WebUILoginView::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView WebUILoginView::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point WebUILoginView::GetDialogPosition(const gfx::Size& size) {
  // Center the widget.
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(widget_size.width() / 2 - size.width() / 2,
                    widget_size.height() / 2 - size.height() / 2);
}

gfx::Size WebUILoginView::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void WebUILoginView::AddObserver(web_modal::ModalDialogHostObserver* observer) {
  if (observer && !observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WebUILoginView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool WebUILoginView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  AccelMap::const_iterator entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end())
    return false;

  if (!web_view())
    return true;

  content::WebUI* web_ui = GetWebUI();
  if (web_ui) {
    base::Value accel_name(entry->second);
    web_ui->CallJavascriptFunctionUnsafe("cr.ui.Oobe.handleAccelerator",
                                         accel_name);
  }

  return true;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::LoadURL(const GURL& url) {
  if (!is_reusing_webview_)
    web_view()->LoadInitialURL(url);
  web_view()->RequestFocus();
}

content::WebUI* WebUILoginView::GetWebUI() {
  return web_view()->web_contents()->GetWebUI();
}

content::WebContents* WebUILoginView::GetWebContents() {
  return web_view()->web_contents();
}

OobeUI* WebUILoginView::GetOobeUI() {
  if (!GetWebUI())
    return nullptr;

  return static_cast<OobeUI*>(GetWebUI()->GetController());
}

void WebUILoginView::OnPostponedShow() {
  set_is_hidden(false);
  OnLoginPromptVisible();
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
  SystemTrayClient::Get()->SetPrimaryTrayVisible(visible);
}

void WebUILoginView::SetUIEnabled(bool enabled) {
  forward_keyboard_event_ = enabled;

  SystemTrayClient::Get()->SetPrimaryTrayEnabled(enabled);
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(web_view());
  web_view()->SetBoundsRect(bounds());

  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void WebUILoginView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return the focus to the web contents.
  web_view()->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
  web_view()->web_contents()->Focus();

  content::WebUI* web_ui = GetWebUI();
  if (web_ui)
    web_ui->CallJavascriptFunctionUnsafe("cr.ui.Oobe.focusReturned");
}

void WebUILoginView::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE:
    case chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN: {
      OnLoginPromptVisible();
      registrar_.RemoveAll();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

views::WebView* WebUILoginView::web_view() {
  return webui_login_.get();
}

void WebUILoginView::SetLockScreenAppFocusCyclerDelegate() {
  if (lock_screen_apps::StateController::IsEnabled()) {
    delegates_lock_screen_app_focus_cycle_ = true;
    lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(this);
  }
}

void WebUILoginView::ClearLockScreenAppFocusCyclerDelegate() {
  if (!delegates_lock_screen_app_focus_cycle_)
    return;
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);
  delegates_lock_screen_app_focus_cycle_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// ChromeKeyboardControllerClient::Observer

void WebUILoginView::OnKeyboardVisibilityChanged(bool visible) {
  if (!GetOobeUI())
    return;
  CoreOobeView* view = GetOobeUI()->GetCoreOobeView();
  view->ShowControlBar(!visible);
  view->SetVirtualKeyboardShown(visible);
}

// WebUILoginView private: -----------------------------------------------------

bool WebUILoginView::HandleContextMenu(
    const content::ContextMenuParams& params) {
#ifndef NDEBUG
  // Do not show the context menu.
  return false;
#else
  return true;
#endif
}

bool WebUILoginView::HandleKeyboardEvent(content::WebContents* source,
                                         const NativeWebKeyboardEvent& event) {
  bool handled = false;
  if (forward_keyboard_event_) {
    // Disable arrow key traversal because arrow keys are handled via
    // accelerator when this view has focus.
    ScopedArrowKeyTraversal arrow_key_traversal(false);

    handled = unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  // Make sure error bubble is cleared on keyboard event. This is needed
  // when the focus is inside an iframe. Only clear on KeyDown to prevent hiding
  // an immediate authentication error (See crbug.com/103643).
  if (event.GetType() == blink::WebInputEvent::kKeyDown) {
    content::WebUI* web_ui = GetWebUI();
    if (web_ui)
      web_ui->CallJavascriptFunctionUnsafe("cr.ui.Oobe.clearErrors");
  }
  return handled;
}

bool WebUILoginView::TakeFocus(content::WebContents* source, bool reverse) {
  // In case of blocked UI (ex.: sign in is in progress)
  // we should not process focus change events.
  if (!forward_keyboard_event_)
    return false;

  // For default tab order, after login UI, try focusing the system tray.
  if (!reverse && MoveFocusToSystemTray(reverse))
    return true;

  // Either if tab order is reversed (in which case system tray focus was not
  // attempted), or system tray focus failed (in which case focusing an app
  // window is preferrable to  focus returning to login UI), try moving focus
  // to the app window.
  if (!lock_screen_app_focus_handler_.is_null()) {
    lock_screen_app_focus_handler_.Run(reverse);
    return true;
  }

  // If initial MoveFocusToSystemTray was skipped due to lock screen app being
  // a preferred option (due to traversal direction), try focusing system tray
  // again.
  if (reverse && MoveFocusToSystemTray(reverse))
    return true;

  // Since neither system tray nor a lock screen app window was focusable, the
  // focus should stay in the login UI.
  AboutToRequestFocusFromTabTraversal(reverse);
  return true;
}

void WebUILoginView::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Note: This is needed for taking photos when selecting new user images
  // and SAML logins. Must work for all user types (including supervised).
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool WebUILoginView::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    content::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

bool WebUILoginView::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void WebUILoginView::RegisterLockScreenAppFocusHandler(
    const LockScreenAppFocusCallback& focus_handler) {
  lock_screen_app_focus_handler_ = focus_handler;
}

void WebUILoginView::UnregisterLockScreenAppFocusHandler() {
  lock_screen_app_focus_handler_.Reset();
}

void WebUILoginView::HandleLockScreenAppFocusOut(bool reverse) {
  if (reverse && MoveFocusToSystemTray(reverse))
    return;

  AboutToRequestFocusFromTabTraversal(reverse);
}

void WebUILoginView::OnFocusLeavingSystemTray(bool reverse) {
  if (!reverse && !lock_screen_app_focus_handler_.is_null()) {
    lock_screen_app_focus_handler_.Run(reverse);
    return;
  }

  AboutToRequestFocusFromTabTraversal(reverse);
}

bool WebUILoginView::MoveFocusToSystemTray(bool reverse) {
  // Focus is accepted, but the Ash system tray is not available in Mash, so
  // exit early. https://crbug.com/782072
  if (features::IsMultiProcessMash())
    return true;

  // The focus should not move to the system tray if voice interaction OOOBE is
  // active.
  if (LoginDisplayHost::default_host() &&
      LoginDisplayHost::default_host()->IsVoiceInteractionOobe()) {
    return false;
  }

  // If shift+tab is used (|reverse| is true) and views-based shelf is shown,
  // focus goes to the shelf widget. If views-based shelf is disabled, focus
  // goes to the system tray, because the web-UI shelf has already been
  // traversed when we reach here.
  ash::Shelf* shelf = ash::Shelf::ForWindow(GetWidget()->GetNativeWindow());
  if (!reverse && ash::ShelfWidget::IsUsingViewsShelf()) {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    ash::Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
    return true;
  }

  ash::RootWindowController* primary_controller =
      ash::RootWindowController::ForWindow(GetWidget()->GetNativeWindow());
  if (!primary_controller->IsSystemTrayVisible())
    return false;

  shelf->GetStatusAreaWidget()
      ->status_area_widget_delegate()
      ->set_default_last_focusable_child(reverse);
  ash::Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  return true;
}

void WebUILoginView::OnLoginPromptVisible() {
  // If we're hidden than will generate this signal once we're shown.
  if (is_hidden_ || webui_visible_) {
    VLOG(1) << "Login WebUI >> not emitting signal, hidden: " << is_hidden_;
    return;
  }
  TRACE_EVENT0("chromeos", "WebUILoginView::OnLoginPromptVisible");
  if (should_emit_login_prompt_visible_) {
    VLOG(1) << "Login WebUI >> login-prompt-visible";
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->EmitLoginPromptVisible();
  }

  webui_visible_ = true;
}

}  // namespace chromeos

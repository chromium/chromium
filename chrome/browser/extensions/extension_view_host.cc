// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host.h"

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color_chooser.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/runtime_data.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::RenderViewHost;
using content::WebContents;
using content::WebContentsObserver;
using web_modal::WebContentsModalDialogManager;

namespace extensions {

// Notifies an ExtensionViewHost when a WebContents is destroyed.
class ExtensionViewHost::AssociatedWebContentsObserver
    : public WebContentsObserver {
 public:
  AssociatedWebContentsObserver(ExtensionViewHost* host,
                                WebContents* web_contents)
      : WebContentsObserver(web_contents), host_(host) {}
  ~AssociatedWebContentsObserver() override {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // Deleting |this| from here is safe.
    host_->SetAssociatedWebContents(NULL);
  }

 private:
  ExtensionViewHost* host_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedWebContentsObserver);
};

ExtensionViewHost::ExtensionViewHost(
    const Extension* extension,
    content::SiteInstance* site_instance,
    const GURL& url,
    ViewType host_type)
    : ExtensionHost(extension, site_instance, url, host_type),
      associated_web_contents_(NULL) {
  // Not used for panels, see PanelHost.
  DCHECK(host_type == VIEW_TYPE_EXTENSION_DIALOG ||
         host_type == VIEW_TYPE_EXTENSION_POPUP);

  // Attach WebContents helpers. Extension tabs automatically get them attached
  // in TabHelpers::AttachTabHelpers, but popups don't.
  // TODO(kalman): How much of TabHelpers::AttachTabHelpers should be here?
  autofill::ChromeAutofillClient::CreateForWebContents(host_contents());
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      host_contents(),
      autofill::ChromeAutofillClient::FromWebContents(host_contents()),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  if (performance_manager::PerformanceManager::IsAvailable()) {
    performance_manager::PerformanceManagerTabHelper::CreateForWebContents(
        host_contents());
  }

  // The popup itself cannot be zoomed, but we must specify a zoom level to use.
  // Otherwise, if a user zooms a page of the same extension, the popup would
  // use the per-origin zoom level.
  if (host_type == VIEW_TYPE_EXTENSION_POPUP) {
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetForWebContents(host_contents());
    zoom_map->SetTemporaryZoomLevel(
        host_contents()->GetRenderViewHost()->GetProcess()->GetID(),
        host_contents()->GetRenderViewHost()->GetRoutingID(),
        zoom_map->GetDefaultZoomLevel());
  }
}

ExtensionViewHost::~ExtensionViewHost() {
  // The hosting WebContents will be deleted in the base class, so unregister
  // this object before it deletes the attached WebContentsModalDialogManager.
  WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(host_contents());
  if (manager)
    manager->SetDelegate(NULL);
}

void ExtensionViewHost::CreateView(Browser* browser) {
  view_ = CreateExtensionView(this, browser);
}

void ExtensionViewHost::SetAssociatedWebContents(WebContents* web_contents) {
  associated_web_contents_ = web_contents;
  if (associated_web_contents_) {
    // Observe the new WebContents for deletion.
    associated_web_contents_observer_.reset(
        new AssociatedWebContentsObserver(this, associated_web_contents_));
  } else {
    associated_web_contents_observer_.reset();
  }
}

bool ExtensionViewHost::UnhandledKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return view_->HandleKeyboardEvent(source, event);
}

// ExtensionHost overrides:

void ExtensionViewHost::OnDidStopFirstLoad() {
  view_->OnLoaded();
}

void ExtensionViewHost::LoadInitialURL() {
  if (!ExtensionSystem::Get(browser_context())->
          runtime_data()->IsBackgroundPageReady(extension())) {
    // Make sure the background page loads before any others.
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                   content::Source<Extension>(extension()));
    return;
  }

  // Popups may spawn modal dialogs, which need positioning information.
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP) {
    WebContentsModalDialogManager::CreateForWebContents(host_contents());
    WebContentsModalDialogManager::FromWebContents(
        host_contents())->SetDelegate(this);
  }

  ExtensionHost::LoadInitialURL();
}

bool ExtensionViewHost::IsBackgroundPage() const {
  DCHECK(view_);
  return false;
}

// content::WebContentsDelegate overrides:

WebContents* ExtensionViewHost::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  // Whitelist the dispositions we will allow to be opened.
  switch (params.disposition) {
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD: {
      // Only allow these from hosts that are bound to a browser (e.g. popups).
      // Otherwise they are not driven by a user gesture.
      Browser* browser = view_->GetBrowser();
      return browser ? browser->OpenURL(params) : nullptr;
    }
    default:
      return nullptr;
  }
}

bool ExtensionViewHost::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  // Block navigations that cause main frame of an extension pop-up (or
  // background page) to navigate to non-extension content (i.e. to web
  // content).
  return !is_main_frame_navigation;
}

content::KeyboardEventProcessingResult
ExtensionViewHost::PreHandleKeyboardEvent(WebContents* source,
                                          const NativeWebKeyboardEvent& event) {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP &&
      event.GetType() == NativeWebKeyboardEvent::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
  }

  // Handle higher priority browser shortcuts such as Ctrl-w.
  Browser* browser = view_->GetBrowser();
  if (browser)
    return browser->PreHandleKeyboardEvent(source, event);

  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool ExtensionViewHost::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP) {
    if (event.GetType() == NativeWebKeyboardEvent::kRawKeyDown &&
        event.windows_key_code == ui::VKEY_ESCAPE) {
      Close();
      return true;
    }
  }
  return UnhandledKeyboardEvent(source, event);
}

bool ExtensionViewHost::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

content::ColorChooser* ExtensionViewHost::OpenColorChooser(
    WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  // Similar to the file chooser below, opening a color chooser requires a
  // visible <input> element to click on. Therefore this code only exists for
  // extensions with a view.
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ExtensionViewHost::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // For security reasons opening a file picker requires a visible <input>
  // element to click on, so this code only exists for extensions with a view.
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}


void ExtensionViewHost::ResizeDueToAutoResize(WebContents* source,
                                              const gfx::Size& new_size) {
  view_->ResizeDueToAutoResize(source, new_size);
}

// content::WebContentsObserver overrides:

void ExtensionViewHost::RenderViewCreated(RenderViewHost* render_view_host) {
  ExtensionHost::RenderViewCreated(render_view_host);
  view_->RenderViewCreated(render_view_host);
}

// web_modal::WebContentsModalDialogManagerDelegate overrides:

web_modal::WebContentsModalDialogHost*
ExtensionViewHost::GetWebContentsModalDialogHost() {
  return this;
}

bool ExtensionViewHost::IsWebContentsVisible(WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

gfx::NativeView ExtensionViewHost::GetHostView() const {
  return view_->GetNativeView();
}

gfx::Point ExtensionViewHost::GetDialogPosition(const gfx::Size& size) {
  if (!GetVisibleWebContents())
    return gfx::Point();
  gfx::Rect bounds = GetVisibleWebContents()->GetViewBounds();
  return gfx::Point(
      std::max(0, (bounds.width() - size.width()) / 2),
      std::max(0, (bounds.height() - size.height()) / 2));
}

gfx::Size ExtensionViewHost::GetMaximumDialogSize() {
  if (!GetVisibleWebContents())
    return gfx::Size();
  return GetVisibleWebContents()->GetViewBounds().size();
}

void ExtensionViewHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
}

void ExtensionViewHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
}

WindowController* ExtensionViewHost::GetExtensionWindowController() const {
  Browser* browser = view_->GetBrowser();
  return browser ? browser->extension_window_controller() : NULL;
}

WebContents* ExtensionViewHost::GetAssociatedWebContents() const {
  return associated_web_contents_;
}

WebContents* ExtensionViewHost::GetVisibleWebContents() const {
  if (associated_web_contents_)
    return associated_web_contents_;
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP)
    return host_contents();
  return NULL;
}

void ExtensionViewHost::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY);
  DCHECK(ExtensionSystem::Get(browser_context())
             ->runtime_data()
             ->IsBackgroundPageReady(extension()));
  LoadInitialURL();
}

}  // namespace extensions

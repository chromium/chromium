// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host.h"

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
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace extensions {

// Notifies an ExtensionViewHost when a WebContents is destroyed.
class ExtensionViewHost::AssociatedWebContentsObserver
    : public content::WebContentsObserver {
 public:
  AssociatedWebContentsObserver(ExtensionViewHost* host,
                                content::WebContents* web_contents)
      : WebContentsObserver(web_contents), host_(host) {}
  AssociatedWebContentsObserver(const AssociatedWebContentsObserver&) = delete;
  AssociatedWebContentsObserver& operator=(
      const AssociatedWebContentsObserver&) = delete;
  ~AssociatedWebContentsObserver() override = default;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // Deleting |this| from here is safe.
    host_->SetAssociatedWebContents(nullptr);
  }

 private:
  ExtensionViewHost* host_;
};

ExtensionViewHost::ExtensionViewHost(const Extension* extension,
                                     content::SiteInstance* site_instance,
                                     const GURL& url,
                                     mojom::ViewType host_type,
                                     Browser* browser)
    : ExtensionHost(extension, site_instance, url, host_type),
      browser_(browser) {
  // Not used for panels, see PanelHost.
  DCHECK(host_type == mojom::ViewType::kExtensionDialog ||
         host_type == mojom::ViewType::kExtensionPopup);

  // The browser should always be associated with the same original profile as
  // this view host. The profiles may not be identical (i.e., one may be the
  // off-the-record version of the other) in the case of a spanning-mode
  // extension creating a popup in an incognito window.
  DCHECK(!browser_ || Profile::FromBrowserContext(browser_context())
                          ->IsSameOrParent(browser_->profile()));

  // Attach WebContents helpers. Extension tabs automatically get them attached
  // in TabHelpers::AttachTabHelpers, but popups don't.
  // TODO(kalman): How much of TabHelpers::AttachTabHelpers should be here?
  autofill::ChromeAutofillClient::CreateForWebContents(host_contents());
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      host_contents(),
      autofill::ChromeAutofillClient::FromWebContents(host_contents()),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

  // The popup itself cannot be zoomed, but we must specify a zoom level to use.
  // Otherwise, if a user zooms a page of the same extension, the popup would
  // use the per-origin zoom level.
  if (host_type == mojom::ViewType::kExtensionPopup) {
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetForWebContents(host_contents());
    zoom_map->SetTemporaryZoomLevel(
        host_contents()
            ->GetMainFrame()
            ->GetProcess()
            ->GetID(),
        host_contents()->GetMainFrame()->GetRenderViewHost()->GetRoutingID(),
        zoom_map->GetDefaultZoomLevel());
  }
}

ExtensionViewHost::~ExtensionViewHost() {
  // The hosting WebContents will be deleted in the base class, so unregister
  // this object before it deletes the attached WebContentsModalDialogManager.
  auto* const manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          host_contents());
  if (manager)
    manager->SetDelegate(nullptr);
}

void ExtensionViewHost::SetAssociatedWebContents(
    content::WebContents* web_contents) {
  associated_web_contents_ = web_contents;
  if (associated_web_contents_) {
    // Observe the new WebContents for deletion.
    associated_web_contents_observer_ =
        std::make_unique<AssociatedWebContentsObserver>(
            this, associated_web_contents_);
  } else {
    associated_web_contents_observer_.reset();
  }
}

bool ExtensionViewHost::UnhandledKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return view_->HandleKeyboardEvent(source, event);
}

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
  if (extension_host_type() == mojom::ViewType::kExtensionPopup) {
    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        host_contents());
    web_modal::WebContentsModalDialogManager::FromWebContents(host_contents())
        ->SetDelegate(this);
  }

  ExtensionHost::LoadInitialURL();
}

bool ExtensionViewHost::IsBackgroundPage() const {
  return false;
}

content::WebContents* ExtensionViewHost::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // Allowlist the dispositions we will allow to be opened.
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
      return browser_ ? browser_->OpenURL(params) : nullptr;
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
ExtensionViewHost::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (IsEscapeInPopup(event))
    return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;

  // Handle higher priority browser shortcuts such as ctrl-w.
  return browser_ ? browser_->PreHandleKeyboardEvent(source, event)
                  : content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool ExtensionViewHost::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (IsEscapeInPopup(event)) {
    Close();
    return true;
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
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  // Similar to the file chooser below, opening a color chooser requires a
  // visible <input> element to click on. Therefore this code only exists for
  // extensions with a view.
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ExtensionViewHost::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // For security reasons opening a file picker requires a visible <input>
  // element to click on, so this code only exists for extensions with a view.
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void ExtensionViewHost::ResizeDueToAutoResize(content::WebContents* source,
                                              const gfx::Size& new_size) {
  view_->ResizeDueToAutoResize(source, new_size);
}

void ExtensionViewHost::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  ExtensionHost::RenderFrameCreated(frame_host);
  view_->RenderFrameCreated(frame_host);
}

web_modal::WebContentsModalDialogHost*
ExtensionViewHost::GetWebContentsModalDialogHost() {
  return this;
}

bool ExtensionViewHost::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

gfx::NativeView ExtensionViewHost::GetHostView() const {
  return view_->GetNativeView();
}

gfx::Point ExtensionViewHost::GetDialogPosition(const gfx::Size& size) {
  auto* const web_contents = GetVisibleWebContents();
  const gfx::Size view_size =
      web_contents ? web_contents->GetViewBounds().size() : gfx::Size();
  return gfx::Rect(view_size - size).CenterPoint();
}

gfx::Size ExtensionViewHost::GetMaximumDialogSize() {
  auto* const web_contents = GetVisibleWebContents();
  return web_contents ? web_contents->GetViewBounds().size() : gfx::Size();
}

void ExtensionViewHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
}

void ExtensionViewHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
}

WindowController* ExtensionViewHost::GetExtensionWindowController() const {
  return browser_ ? browser_->extension_window_controller() : nullptr;
}

content::WebContents* ExtensionViewHost::GetAssociatedWebContents() const {
  return associated_web_contents_;
}

content::WebContents* ExtensionViewHost::GetVisibleWebContents() const {
  if (associated_web_contents_)
    return associated_web_contents_;
  return (extension_host_type() == mojom::ViewType::kExtensionPopup)
             ? host_contents()
             : nullptr;
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

bool ExtensionViewHost::IsEscapeInPopup(
    const content::NativeWebKeyboardEvent& event) const {
  return extension_host_type() == mojom::ViewType::kExtensionPopup &&
         event.GetType() ==
             content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

}  // namespace extensions

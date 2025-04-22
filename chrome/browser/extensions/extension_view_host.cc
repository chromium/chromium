// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_util.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace extensions {

ExtensionViewHost::Delegate::Delegate() = default;
ExtensionViewHost::Delegate::~Delegate() = default;

ExtensionViewHost::ExtensionViewHost(
    const Extension* extension,
    content::SiteInstance* site_instance,
    content::BrowserContext* browser_context_param,
    const GURL& url,
    mojom::ViewType host_type,
    std::unique_ptr<Delegate> delegate)
    : ExtensionHost(extension,
                    site_instance,
                    browser_context_param,
                    url,
                    host_type),
      delegate_(std::move(delegate)) {
  // Not used for panels, see PanelHost.
  DCHECK(host_type == mojom::ViewType::kExtensionPopup ||
         host_type == mojom::ViewType::kExtensionSidePanel);

  // Attach WebContents helpers. Extension tabs automatically get them attached
  // in TabHelpers::AttachTabHelpers, but popups don't.
  // TODO(kalman): How much of TabHelpers::AttachTabHelpers should be here?
  autofill::ChromeAutofillClient::CreateForWebContents(host_contents());
}

ExtensionViewHost::~ExtensionViewHost() {
  // The hosting WebContents will be deleted in the base class, so unregister
  // this object before it deletes the attached WebContentsModalDialogManager.
  auto* const manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          host_contents());
  if (manager) {
    manager->SetDelegate(nullptr);
  }
  for (auto& observer : modal_dialog_host_observers_) {
    observer.OnHostDestroying();
  }
}

bool ExtensionViewHost::UnhandledKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return view_->HandleKeyboardEvent(source, event);
}

void ExtensionViewHost::OnDidStopFirstLoad() {
  view_->OnLoaded();
}

void ExtensionViewHost::LoadInitialURL() {
  if (process_util::GetPersistentBackgroundPageState(*extension(),
                                                     browser_context()) ==
      process_util::PersistentBackgroundPageState::kNotReady) {
    // Make sure the background page loads before any others.
    host_registry_observation_.Observe(
        ExtensionHostRegistry::Get(browser_context()));
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

void ExtensionViewHost::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  ExtensionHost::ReadyToCommitNavigation(navigation_handle);

  // The popup itself cannot be zoomed, but we must specify a zoom level to use.
  // Otherwise, if a user zooms a page of the same extension, the popup would
  // use the per-origin zoom level.
  // We do this right before commit (rather than in the constructor) because the
  // RenderFrameHost may be swapped during the creation/load process.
  if (extension_host_type() == mojom::ViewType::kExtensionPopup) {
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetForWebContents(host_contents());
    zoom_map->SetTemporaryZoomLevel(
        host_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        zoom_map->GetDefaultZoomLevel());
  }
}

content::WebContents* ExtensionViewHost::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
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
      return delegate_->OpenURL(params, std::move(navigation_handle_callback));
    }
    default:
      return nullptr;
  }
}

bool ExtensionViewHost::ShouldAllowRendererInitiatedCrossProcessNavigation(
    bool is_outermost_main_frame_navigation) {
  // Block navigations that cause main frame of an extension pop-up (or
  // background page) to navigate to non-extension content (i.e. to web
  // content).
  return !is_outermost_main_frame_navigation;
}

content::KeyboardEventProcessingResult
ExtensionViewHost::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (IsEscapeInPopup(event))
    return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;

  // Handle higher priority browser shortcuts such as ctrl-w.
  return delegate_->PreHandleKeyboardEvent(source, event);
}

bool ExtensionViewHost::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
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

void ExtensionViewHost::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // For security reasons opening a file picker requires a visible <input>
  // element to click on, so this code only exists for extensions with a view.
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

std::unique_ptr<content::EyeDropper> ExtensionViewHost::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return delegate_->OpenEyeDropper(frame, listener);
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
  modal_dialog_host_observers_.AddObserver(observer);
}

void ExtensionViewHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.RemoveObserver(observer);
}

WindowController* ExtensionViewHost::GetExtensionWindowController() const {
  return delegate_->GetExtensionWindowController();
}

content::WebContents* ExtensionViewHost::GetVisibleWebContents() const {
  return (extension_host_type() == mojom::ViewType::kExtensionPopup)
             ? host_contents()
             : nullptr;
}

void ExtensionViewHost::OnExtensionHostDocumentElementAvailable(
    content::BrowserContext* host_browser_context,
    ExtensionHost* extension_host) {
  DCHECK(extension_host->extension());
  if (host_browser_context != browser_context() ||
      extension_host->extension() != extension() ||
      extension_host->extension_host_type() !=
          mojom::ViewType::kExtensionBackgroundPage) {
    return;
  }

  DCHECK_EQ(process_util::PersistentBackgroundPageState::kReady,
            process_util::GetPersistentBackgroundPageState(*extension(),
                                                           browser_context()));
  // We only needed to wait for the background page to load, so stop observing.
  host_registry_observation_.Reset();
  LoadInitialURL();
}

bool ExtensionViewHost::IsEscapeInPopup(
    const input::NativeWebKeyboardEvent& event) const {
  return extension_host_type() == mojom::ViewType::kExtensionPopup &&
         event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

}  // namespace extensions

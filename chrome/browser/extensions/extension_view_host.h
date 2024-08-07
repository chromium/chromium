// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_registry.h"

class Browser;

namespace content {
class SiteInstance;
class WebContents;
}

namespace extensions {

class ExtensionView;

// The ExtensionHost for an extension that backs a view in the browser UI. For
// example, this could be an extension popup or dialog, but not a background
// page.
class ExtensionViewHost
    : public ExtensionHost,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost,
      public ExtensionHostRegistry::Observer {
 public:
  // |browser| may be null, since extension views may be bound to TabContents
  // hosted in ExternalTabContainer objects, which do not instantiate Browsers.
  ExtensionViewHost(const Extension* extension,
                    content::SiteInstance* site_instance,
                    const GURL& url,
                    mojom::ViewType host_type,
                    Browser* browser);

  ExtensionViewHost(const ExtensionViewHost&) = delete;
  ExtensionViewHost& operator=(const ExtensionViewHost&) = delete;

  ~ExtensionViewHost() override;

  void set_view(ExtensionView* view) { view_ = view; }
  ExtensionView* view() { return view_; }

  // Returns the browser associated with this ExtensionViewHost.
  virtual Browser* GetBrowser();

  // ExtensionHost
  void OnDidStopFirstLoad() override;
  void LoadInitialURL() override;
  bool IsBackgroundPage() const override;

  // content::WebContentsDelegate
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_outermost_main_frame_navigation) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* frame_host) override;

  // web_modal::WebContentsModalDialogManagerDelegate
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // extensions::ExtensionFunctionDispatcher::Delegate
  WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetVisibleWebContents() const override;

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostDocumentElementAvailable(
      content::BrowserContext* browser_context,
      ExtensionHost* extension_host) override;

 private:
  // Returns whether the provided event is a raw escape keypress in a
  // mojom::ViewType::kExtensionPopup.
  bool IsEscapeInPopup(const input::NativeWebKeyboardEvent& event) const;

  // Handles keyboard events that were not handled by HandleKeyboardEvent().
  // Platform specific implementation may override this method to handle the
  // event in platform specific way. Returns whether the events are handled.
  bool UnhandledKeyboardEvent(content::WebContents* source,
                              const input::NativeWebKeyboardEvent& event);

  // The browser associated with the ExtensionView, if any. Note: since this
  // ExtensionViewHost could be associated with a browser even if `browser_` is
  // null (see ExtensionSidePanelViewHost), this variable should not be used
  // directly. Instead, use GetBrowser().
  raw_ptr<Browser> browser_;

  // View that shows the rendered content in the UI.
  raw_ptr<ExtensionView, DanglingUntriaged> view_ = nullptr;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observers_;

  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_

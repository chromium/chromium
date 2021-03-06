// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_
#define CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_

#include "chrome/browser/bad_message.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {

class RenderFrameHost;
}  // namespace content

namespace chrome {
namespace internal {

// The mechanism implemented by the PopulateChrome*FrameBinders() functions
// below will replace interface registries and binders used for handling
// InterfaceProvider's GetInterface() calls (see crbug.com/718652).

// PopulateChromeFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces.
void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host);

// PopulateChromeWebUIFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for chrome-specific document-scoped
// interfaces used from WebUI pages (e.g. chrome://bluetooth-internals).
void PopulateChromeWebUIFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map);

template <typename Interface, int N, typename... Subclasses>
struct BinderHelper;

template <typename Interface, typename WebUIControllerSubclass>
bool SafeDownCastAndBindInterface(content::WebUI* web_ui,
                                  mojo::PendingReceiver<Interface>& receiver) {
  // Performs a safe downcast to the concrete WebUIController subclass.
  WebUIControllerSubclass* concrete_controller =
      web_ui ? web_ui->GetController()->GetAs<WebUIControllerSubclass>()
             : nullptr;

  if (!concrete_controller)
    return false;

  // Fails to compile if |Subclass| does not implement the appropriate overload
  // for |Interface|.
  concrete_controller->BindInterface(std::move(receiver));
  return true;
}

template <typename Interface, int N, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, N, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(content::WebUI* web_ui,
                            mojo::PendingReceiver<Interface> receiver) {
    // Try a different subclass if the current one is not the right
    // WebUIController for the current WebUI page, and only fail if none of the
    // passed subclasses match.
    if (!SafeDownCastAndBindInterface<Interface, Subclass>(web_ui, receiver)) {
      return BinderHelper<Interface, N - 1, std::tuple<Subclasses...>>::
          BindInterface(web_ui, std::move(receiver));
    }
    return true;
  }
};

template <typename Interface, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, 0, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(content::WebUI* web_ui,
                            mojo::PendingReceiver<Interface> receiver) {
    return SafeDownCastAndBindInterface<Interface, Subclass>(web_ui, receiver);
  }
};

// Registers a binder in |map| that binds |Interface| iff the RenderFrameHost
// has a WebUIController among type |WebUIControllerSubclasses|.
template <typename Interface, typename... WebUIControllerSubclasses>
void RegisterWebUIControllerInterfaceBinder(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<Interface>(
      base::BindRepeating([](content::RenderFrameHost* host,
                             mojo::PendingReceiver<Interface> receiver) {
        // This is expected to be called only for main frames.
        if (host->GetParent()) {
          ReceivedBadMessage(
              host->GetProcess(),
              bad_message::BadMessageReason::RFH_INVALID_WEB_UI_CONTROLLER);
          return;
        }

        const int size = sizeof...(WebUIControllerSubclasses);
        bool is_bound = BinderHelper<Interface, size - 1,
                                     std::tuple<WebUIControllerSubclasses...>>::
            BindInterface(host->GetWebUI(), std::move(receiver));

        // This is expected to be called only for the right WebUI pages matching
        // the same WebUI associated to the RenderFrameHost.
        if (!is_bound) {
          ReceivedBadMessage(
              host->GetProcess(),
              bad_message::BadMessageReason::RFH_INVALID_WEB_UI_CONTROLLER);
          return;
        }
      }));
}

}  // namespace internal
}  // namespace chrome

#endif  // CHROME_BROWSER_CHROME_BROWSER_INTERFACE_BINDERS_H_

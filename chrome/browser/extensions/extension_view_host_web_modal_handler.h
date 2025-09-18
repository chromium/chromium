// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_WEB_MODAL_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_WEB_MODAL_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

namespace content {
class WebContents;
}

namespace extensions {

// Sets up web contents modal dialogs for ExtensionViewHost.
class ExtensionViewHostWebModalHandler
    : public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  ExtensionViewHostWebModalHandler(content::WebContents* web_contents,
                                   gfx::NativeView view);
  ExtensionViewHostWebModalHandler(const ExtensionViewHostWebModalHandler&) =
      delete;
  ExtensionViewHostWebModalHandler& operator=(
      const ExtensionViewHostWebModalHandler&) = delete;
  ~ExtensionViewHostWebModalHandler() override;

  // web_modal::WebContentsModalDialogManagerDelegate
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  bool ShouldConstrainDialogBoundsByHost() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
  const gfx::NativeView view_;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_WEB_MODAL_HANDLER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_SCOPED_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_GLIC_WIDGET_SCOPED_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace glic {

// Attaches a `WebContentsModalDialogManagerDelegate` to a `WebContents`'s
// `WebContentsModalDialogManager` and ensures it is safely cleared when
// switching `WebContents` or upon destruction of the delegate/embedder.
//
// In NoWebview mode, Glic swaps active WebContents (between the remote guest
// and the loading/error overlay). This helper guarantees that the delegate is
// detached from the previous WebContents before attaching to the new one,
// preventing dangling raw_ptr crashes if an inactive WebContents is destroyed
// after the embedder.
class ScopedModalDialogManagerDelegate : public content::WebContentsObserver {
 public:
  explicit ScopedModalDialogManagerDelegate(
      web_modal::WebContentsModalDialogManagerDelegate* delegate);
  ScopedModalDialogManagerDelegate(const ScopedModalDialogManagerDelegate&) =
      delete;
  ScopedModalDialogManagerDelegate& operator=(
      const ScopedModalDialogManagerDelegate&) = delete;
  ~ScopedModalDialogManagerDelegate() override;

  // Detaches from any currently observed WebContents and attaches `delegate_`
  // to `new_web_contents`. Passing nullptr cleanly detaches without attaching.
  void SetWebContents(content::WebContents* new_web_contents);

 private:
  void Reset();

  raw_ptr<web_modal::WebContentsModalDialogManagerDelegate> delegate_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_SCOPED_MODAL_DIALOG_MANAGER_DELEGATE_H_

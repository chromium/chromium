// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_SCOPED_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_GLIC_WIDGET_SCOPED_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace glic {

// Attaches a `WebContentsModalDialogManagerDelegate` to `WebContents`
// modal dialog managers and ensures it is safely cleared when WebContents
// are removed or upon destruction of the delegate/embedder.
//
// In NoWebview mode, Glic hosts both the guest WebContents and the
// loading/error WebUI overlay. This helper supports managing delegates for
// both contents simultaneously, preventing modal dialog crashes and deadlocks.
class ScopedModalDialogManagerDelegate {
 public:
  explicit ScopedModalDialogManagerDelegate(
      web_modal::WebContentsModalDialogManagerDelegate* delegate);
  ScopedModalDialogManagerDelegate(const ScopedModalDialogManagerDelegate&) =
      delete;
  ScopedModalDialogManagerDelegate& operator=(
      const ScopedModalDialogManagerDelegate&) = delete;
  ~ScopedModalDialogManagerDelegate();

  // Attaches `delegate_` to `web_contents`'s WebContentsModalDialogManager
  // (creating the manager if needed) and begins observing its lifetime.
  void AddWebContents(content::WebContents* web_contents);

  // Detaches `delegate_` from `web_contents` and stops observing it.
  void RemoveWebContents(content::WebContents* web_contents);

  // Detaches `delegate_` from all observed WebContents.
  void Reset();

 private:
  class WebContentsWatcher : public content::WebContentsObserver {
   public:
    explicit WebContentsWatcher(content::WebContents* web_contents);
    ~WebContentsWatcher() override;
  };

  raw_ptr<web_modal::WebContentsModalDialogManagerDelegate> delegate_;
  std::vector<std::unique_ptr<WebContentsWatcher>> watchers_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_SCOPED_MODAL_DIALOG_MANAGER_DELEGATE_H_

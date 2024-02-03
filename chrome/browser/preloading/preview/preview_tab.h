// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_
#define CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/preview/preview_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/accelerators/accelerator.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class PrerenderHandle;
class PreviewCancelReason;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

class PreviewZoomController;

// Hosts a WebContents for preview until a user decides to navigate to it.
class PreviewTab final : public content::WebContentsDelegate,
                         public ui::AcceleratorTarget {
 public:
  PreviewTab(PreviewManager* preview_manager,
             content::WebContents& parent,
             const GURL& url);
  ~PreviewTab() override;

  PreviewTab(const PreviewTab&) = delete;
  PreviewTab& operator=(const PreviewTab&) = delete;

  PreviewZoomController* preview_zoom_controller() const {
    return preview_zoom_controller_.get();
  }

  // Opens the previewed WebContents as a new tab.
  //
  // This attaches remaining all tab helpers as for the ordinal navigation,
  // promote WebContents as a tab, and activates the page.
  void PromoteToNewTab(content::WebContents& initiator_web_contents);
  // This performs activation steps for tab promotion. This will relax the
  // capability control, and send an IPC to relevant renderers  to perform
  // the prerendering activation algorithm that updates document.prerendering
  // and runs queued suspended tasks such as resolving promises, releasing
  // AudioContext, etc.
  // This is not fully implemented, and the progress is tracked at b:305000959.
  void Activate(base::WeakPtr<content::WebContents> web_contents);

  // content::WebContentsDelegate implementation:
  void CancelPreview(content::PreviewCancelReason reason) override;

  base::WeakPtr<content::WebContents> GetWebContents();

 private:
  class PreviewWidget;

  void AttachTabHelpersForInit();

  void InitWindow(content::WebContents& parent);

  // content::WebContentsDelegate implementation:
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;

  void RegisterKeyboardAccelerators();

  // ui::AcceleratorTarget implementation:
  bool CanHandleAccelerators() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<PreviewWidget> widget_;
  std::unique_ptr<views::WebView> view_;
  std::unique_ptr<PreviewZoomController> preview_zoom_controller_;
  // TODO(b:298347467): Design the actual promotion sequence and move this to
  // PrerenderManager.
  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
  GURL url_;
  std::optional<content::PreviewCancelReason> cancel_reason_ = std::nullopt;
  // A mapping between accelerators and command IDs.
  base::flat_map<ui::Accelerator, int> accelerator_table_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_

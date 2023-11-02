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
#include "url/gurl.h"

namespace content {
class WebContents;
class PrerenderHandle;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// Hosts a WebContents for preview until a user decides to navigate to it.
class PreviewTab final : public content::WebContentsDelegate {
 public:
  PreviewTab(PreviewManager* preview_manager,
             content::WebContents& parent,
             const GURL& url);
  ~PreviewTab() override;

  PreviewTab(const PreviewTab&) = delete;
  PreviewTab& operator=(const PreviewTab&) = delete;

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

  base::WeakPtr<content::WebContents> GetWebContents();

 private:
  class PreviewWidget;
  class WebContentsObserver;

  void AttachTabHelpersForInit();

  void InitWindow(content::WebContents& parent);

  // content::WebCopntentsDelegate implementation:
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;
  bool IsInPreviewMode() const override;
  void CancelPreviewByMojoBinderPolicy(
      const std::string& interface_name) override;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebContentsObserver> observer_;
  std::unique_ptr<PreviewWidget> widget_;
  std::unique_ptr<views::WebView> view_;
  // TODO(b:298347467): Design the actual promotion sequence and move this to
  // PrerenderManager.
  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
  GURL url_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_
#define CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_

#include <memory>

#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class PrerenderHandle;
}  // namespace content

namespace views {
class WebView;
class Widget;
}  // namespace views

// Hosts a WebContents for preview until a user decides to navigate to it.
class PreviewTab final : public content::WebContentsDelegate {
 public:
  PreviewTab(content::WebContents& parent, const GURL& url);
  ~PreviewTab() override;

  PreviewTab(const PreviewTab&) = delete;
  PreviewTab& operator=(const PreviewTab&) = delete;

  void Show();

 private:
  class WebContentsObserver;

  void AttachTabHelpersForInit();

  void InitWindow(content::WebContents& parent);

  // content::WebCopntentsDelegate implementation:
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::WebView> view_;
  // TODO(b:298347467): Design the actual promotion sequence and move this to
  // PrerenderManager.
  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
  GURL url_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TAB_H_

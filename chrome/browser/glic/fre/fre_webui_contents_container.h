// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_FRE_WEBUI_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_GLIC_FRE_FRE_WEBUI_CONTENTS_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {
class GlicFreController;

// Owns the `WebContents` that houses Glic's FRE WebUI.
class FreWebUIContentsContainer : public content::WebContentsDelegate,
                                  public content::WebContentsObserver {
 public:
  FreWebUIContentsContainer(Profile* profile,
                            views::WebView* web_view,
                            GlicFreController* fre_controller);
  ~FreWebUIContentsContainer() override;

  FreWebUIContentsContainer(const FreWebUIContentsContainer&) = delete;
  FreWebUIContentsContainer& operator=(const FreWebUIContentsContainer&) =
      delete;

  // content::WebContentsDelegate implementation:
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  const std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<views::WebView> fre_web_view_;
  raw_ptr<GlicFreController> fre_controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_FRE_WEBUI_CONTENTS_CONTAINER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_VIEW_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/view.h"

namespace content {
class BrowserContext;
}

namespace views {
class WebView;
}

namespace chromeos {

class FirstRunActor;
class FirstRunController;

// WebUI view used for first run tutorial.
class FirstRunView : public views::View,
                     public content::WebContentsDelegate {
 public:
  FirstRunView();
  void Init(content::BrowserContext* context, FirstRunController* controller);
  FirstRunActor* GetActor();

  // Overriden from views::View.
  void RequestFocus() override;

  content::WebContents* GetWebContents();

 private:
  // Overriden from content::WebContentsDelegate.
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

  views::WebView* web_view_ = nullptr;
  FirstRunController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FirstRunView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_VIEW_H_

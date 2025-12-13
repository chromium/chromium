// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

class Browser;
class ContentsWebView;

namespace glic {

class ContextSharingBorderView;

class ContextSharingBorderViewController {
 public:
  virtual ~ContextSharingBorderViewController() = default;

  // Initialization. Starts observing the state of the browser.
  virtual void Initialize(ContextSharingBorderView* border_view,
                          ContentsWebView* contents_web_view,
                          Browser* browser) = 0;

  // Returns the ContentWebView around which the border is to be created.
  virtual ContentsWebView* contents_web_view() = 0;

  // Returns whether the currently shown UI is in side panel mode.
  // For contextual tasks, it will be always true. For glic, it will return
  // GlicEnabling::IsMultiInstanceEnabled.
  virtual bool IsSidePanelOpen() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_CONTROLLER_H_

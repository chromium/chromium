// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_CONTEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_CONTEXT_MENU_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

namespace content {
class WebContents;
}

class PrintPreviewContextMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit PrintPreviewContextMenuObserver(content::WebContents* contents);
  ~PrintPreviewContextMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;

 private:
  bool IsPrintPreviewDialog();

  content::WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewContextMenuObserver);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_CONTEXT_MENU_OBSERVER_H_

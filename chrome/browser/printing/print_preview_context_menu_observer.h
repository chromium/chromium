// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_CONTEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_CONTEXT_MENU_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

namespace content {
class WebContents;
}

class PrintPreviewContextMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit PrintPreviewContextMenuObserver(content::WebContents* contents);

  PrintPreviewContextMenuObserver(const PrintPreviewContextMenuObserver&) =
      delete;
  PrintPreviewContextMenuObserver& operator=(
      const PrintPreviewContextMenuObserver&) = delete;

  ~PrintPreviewContextMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;

 private:
  bool IsPrintPreviewDialog();

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> contents_;
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_CONTEXT_MENU_OBSERVER_H_

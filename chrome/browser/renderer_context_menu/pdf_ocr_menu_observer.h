// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_PDF_OCR_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_PDF_OCR_MENU_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

class RenderViewContextMenuProxy;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with
// PdfOcrUserSelection enum in //tools/metrics/histograms/enums.xml.
enum class PdfOcrUserSelection {
  kDeprecated_TurnOnOnceFromContextMenu = 0,
  kTurnOnAlwaysFromContextMenu = 1,
  kTurnOffFromContextMenu = 2,
  kTurnOnAlwaysFromMoreActions = 3,
  kTurnOffFromMoreActions = 4,
  kTurnOnAlwaysFromSettings = 5,
  kTurnOffFromSettings = 6,
  kMaxValue = kTurnOffFromSettings,
};

// An observer that listens to events from the RenderViewContextMenu class and
// shows the PDF OCR menu if a screen reader is enabled.
class PdfOcrMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit PdfOcrMenuObserver(RenderViewContextMenuProxy* proxy);

  PdfOcrMenuObserver(const PdfOcrMenuObserver&) = delete;
  PdfOcrMenuObserver& operator=(const PdfOcrMenuObserver&) = delete;

  ~PdfOcrMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  // The interface to add a context-menu item and update it. This class uses
  // this interface to avoid accessing context-menu items directly.
  raw_ptr<RenderViewContextMenuProxy> proxy_;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_PDF_OCR_MENU_OBSERVER_H_

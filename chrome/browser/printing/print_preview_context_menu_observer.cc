// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_context_menu_observer.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"

PrintPreviewContextMenuObserver::PrintPreviewContextMenuObserver(
    content::WebContents* contents) : contents_(contents) {
}

PrintPreviewContextMenuObserver::~PrintPreviewContextMenuObserver() {
}

bool PrintPreviewContextMenuObserver::IsPrintPreviewDialog() {
  auto* controller = printing::PrintPreviewDialogController::GetInstance();
  CHECK(controller);
  return !!controller->GetPrintPreviewForContents(contents_);
}

bool PrintPreviewContextMenuObserver::IsCommandIdSupported(int command_id) {
  switch (command_id) {
    case IDC_PRINT:
    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
      return IsPrintPreviewDialog();

    default:
      return false;
  }
}

bool PrintPreviewContextMenuObserver::IsCommandIdEnabled(int command_id) {
  switch (command_id) {
    case IDC_PRINT:
    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
      return false;

    default:
      NOTREACHED_IN_MIGRATION();
      return true;
  }
}

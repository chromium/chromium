// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/scoped_tab_print_target_override.h"

#include "base/check.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif

namespace printing {

DEFINE_USER_DATA(ScopedTabPrintTargetOverride);

ScopedTabPrintTargetOverride::ScopedTabPrintTargetOverride(
    tabs::TabInterface& tab,
    content::WebContents& target)
    : target_(target.GetWeakPtr()),
      scoped_user_data_(tab.GetUnownedUserDataHost(), *this) {}

ScopedTabPrintTargetOverride::~ScopedTabPrintTargetOverride() {
  if (!target_) {
    return;
  }
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // `target_` is no longer the tab's print target, so cancel any print preview
  // it started, as happens when a tab closes mid-print.
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  dialog_controller->CancelPrintPreview(target_.get());
#endif
}

}  // namespace printing

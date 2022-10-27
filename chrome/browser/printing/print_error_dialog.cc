// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_error_dialog.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

struct ErrorDialogOverride {
  base::RepeatingClosure show_dialog;
};

ErrorDialogOverride& GetErrorDialogOverride() {
  static base::NoDestructor<ErrorDialogOverride> error_dialog_override;
  return *error_dialog_override;
}

void ShowPrintErrorDialogTask() {
  if (GetErrorDialogOverride().show_dialog) {
    GetErrorDialogOverride().show_dialog.Run();
    return;
  }

  Browser* browser = chrome::FindLastActive();
  chrome::ShowWarningMessageBox(
      browser ? browser->window()->GetNativeWindow() : gfx::kNullNativeWindow,
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_TITLE_TEXT),
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));
}

}  // namespace

void ShowPrintErrorDialog() {
  // Nested loop may destroy caller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ShowPrintErrorDialogTask));
}

void SetShowPrintErrorDialogForTest(base::RepeatingClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetErrorDialogOverride().show_dialog = std::move(callback);
}

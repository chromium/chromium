// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_error_dialog.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void ShowPrintErrorDialogTask() {
  Browser* browser = chrome::FindLastActive();
  chrome::ShowWarningMessageBox(
      browser ? browser->window()->GetNativeWindow() : gfx::kNullNativeWindow,
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_TITLE_TEXT),
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));
}

}  // namespace

void ShowPrintErrorDialog() {
  // Nested loop may destroy caller.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ShowPrintErrorDialogTask));
}

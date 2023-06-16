// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_error_dialog.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif

namespace {

base::RepeatingClosure& GetErrorDialogOverride() {
  static base::NoDestructor<base::RepeatingClosure> error_dialog_override;
  return *error_dialog_override;
}

void ShowPrintErrorDialogTask(const std::u16string& title,
                              const std::u16string& message) {
  // Runs always on the UI thread.
  static bool is_dialog_shown = false;
  if (is_dialog_shown) {
    return;
  }
  // Block opening dialog from nested task.
  base::AutoReset<bool> auto_reset(&is_dialog_shown, true);

  base::RepeatingClosure& error_dialog_override = GetErrorDialogOverride();
  if (error_dialog_override) {
    error_dialog_override.Run();
    return;
  }

  gfx::NativeWindow window = gfx::NativeWindow();
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    window = browser->window()->GetNativeWindow();
  }
#endif
  chrome::ShowWarningMessageBox(window, title, message);
}

void ShowPrintErrorDialog(const std::u16string& title,
                          const std::u16string& message) {
  PRINTER_LOG(ERROR) << message;

  // Nested loop may destroy caller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ShowPrintErrorDialogTask, title, message));
}

}  // namespace

void ShowPrintErrorDialogForInvalidPrinterError() {
  ShowPrintErrorDialog(
      std::u16string(),
      l10n_util::GetStringUTF16(IDS_PRINT_INVALID_PRINTER_SETTINGS));
}

void ShowPrintErrorDialogForGenericError() {
  ShowPrintErrorDialog(
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_TITLE_TEXT),
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));
}

void SetShowPrintErrorDialogForTest(base::RepeatingClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetErrorDialogOverride() = std::move(callback);
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_
#define CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_

#include "base/functional/callback_forward.h"

// Functions related to showing error dialogs for printing errors. Whether a
// dialog actually appears depends on the platform. Regardless of the platform,
// error dialogs additionally create PRINTER_LOG() entries.

// Shows a window-modal error when a selected printer is invalid.
void ShowPrintErrorDialogForInvalidPrinterError();

// Shows a window-modal error when printing failed for some unknown reason.
void ShowPrintErrorDialogForGenericError();

// Allows tests to override the error dialogs. Instead of displaying a
// window-modal dialog that can block test completion, runs `callback` instead.
// Must be called from the UI thread.
void SetShowPrintErrorDialogForTest(base::RepeatingClosure callback);

#endif  // CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_
#define CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_

#include "base/functional/callback_forward.h"

// Shows a window-modal error that printing failed for some unknown reason.
void ShowPrintErrorDialog();

// Provide callback for testing purposes.  Allows test framework to be notified
// of a printer error dialog event without displaying a window-modal dialog
// that would block testing completion.  Must be called from the UI thread.
void SetShowPrintErrorDialogForTest(base::RepeatingClosure callback);

#endif  // CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_

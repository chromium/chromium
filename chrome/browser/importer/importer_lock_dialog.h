// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_LOCK_DIALOG_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_LOCK_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "chrome/grit/generated_resources.h"
#include "ui/gfx/native_widget_types.h"

namespace importer {

// This function is called by an ImporterHost, and presents a warning dialog
// about the browser whose data is being imported is still running and has to
// be closed. The default messages are for the the Firefox profile importer,
// downstream embedders can specify tailored messages in their implementations.
// After closing the dialog, the ImportHost receives a callback
// with the message either to skip the import, or to continue the process.
void ShowImportLockDialog(gfx::NativeWindow parent,
                          base::OnceCallback<void(bool)> callback,
                          int importer_lock_title_id = IDS_IMPORTER_LOCK_TITLE,
                          int importer_lock_text_id = IDS_IMPORTER_LOCK_TEXT);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_LOCK_DIALOG_H_

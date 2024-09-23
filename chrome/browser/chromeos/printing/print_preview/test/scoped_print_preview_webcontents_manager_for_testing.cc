// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/test/scoped_print_preview_webcontents_manager_for_testing.h"

#include "chrome/browser/chromeos/printing/print_preview/print_preview_webcontents_manager.h"

namespace chromeos {

ScopedPrintPreviewWebContentsManagerForTesting::
    ScopedPrintPreviewWebContentsManagerForTesting(
        PrintPreviewWebcontentsManager* test_manager) {
  PrintPreviewWebcontentsManager::SetInstanceForTesting(test_manager);
}

ScopedPrintPreviewWebContentsManagerForTesting::
    ~ScopedPrintPreviewWebContentsManagerForTesting() {
  PrintPreviewWebcontentsManager::ResetInstanceForTesting();
}

}  // namespace chromeos

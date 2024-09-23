// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_TEST_SCOPED_PRINT_PREVIEW_WEBCONTENTS_MANAGER_FOR_TESTING_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_TEST_SCOPED_PRINT_PREVIEW_WEBCONTENTS_MANAGER_FOR_TESTING_H_

#include "chrome/browser/chromeos/printing/print_preview/print_preview_webcontents_manager.h"

namespace chromeos {

// Helper class to automatically set and reset the global
// `PrintPreviewWebcontentsManager` instance.
class ScopedPrintPreviewWebContentsManagerForTesting {
 public:
  explicit ScopedPrintPreviewWebContentsManagerForTesting(
      PrintPreviewWebcontentsManager* manager);
  ~ScopedPrintPreviewWebContentsManagerForTesting();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_TEST_SCOPED_PRINT_PREVIEW_WEBCONTENTS_MANAGER_FOR_TESTING_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_MANAGER_H_

#include "base/unguessable_token.h"

namespace chromeos {

// Manages a 1:1 relationship between a printing source's webcontents and a
// base::UnguessableToken. Each token represent a webcontent and is used as a
// proxy for determining which print preview is relevant.
// Communicates to ash via crosapi.
class PrintPreviewWebcontentsManager {
 public:
  PrintPreviewWebcontentsManager() = default;
  PrintPreviewWebcontentsManager(const PrintPreviewWebcontentsManager&) =
      delete;
  PrintPreviewWebcontentsManager& operator=(
      const PrintPreviewWebcontentsManager&) = delete;
  ~PrintPreviewWebcontentsManager() = default;

  void GetPreview(base::UnguessableToken token);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_WEBCONTENTS_MANAGER_H_

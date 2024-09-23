// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINTING_INIT_CROS_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINTING_INIT_CROS_H_

#include "printing/buildflags/buildflags.h"

namespace content {
class WebContents;
}

namespace chromeos::printing {

// Initialize printing related classes for a WebContents.
void InitializePrintingForWebContents(content::WebContents* web_contents);

}  // namespace chromeos::printing

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINTING_INIT_CROS_H_

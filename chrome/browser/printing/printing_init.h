// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTING_INIT_H_
#define CHROME_BROWSER_PRINTING_PRINTING_INIT_H_

#include "printing/buildflags/buildflags.h"

namespace content {
class WebContents;
}

namespace printing {

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Perform an early launch of the Print Backend service, if appropriate.  The
// actual launch does not happen immediately, but is scheduled to start after
// the browser has completed its startup sequence.
void EarlyStartPrintBackendService();
#endif

// Initialize printing related classes for a WebContents.
void InitializePrintingForWebContents(content::WebContents* web_contents);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTING_INIT_H_

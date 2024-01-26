// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PRINT_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PRINT_UTILS_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"

namespace content {
class WebContents;
}

namespace enterprise_data_protection {

// Represents context for the kind of print workflow that needs to check if
// scanning should happen. This is to control the timing at which scanning
// occurs.
//
//           +-------------------#3-----------------+
//           |                                      V
//   +---------+         +--------+            +----------+        +-------+
//   | Preview | --#2--> | System | ---------> | Print    | --#4-> | Print |
//   | dialog  |         | dialog | --+        | document | --#5-> | job   |
//   +---------+         +--------+   |        +----------+        +-------+
//                          ^         |
// ------#0-----------------+         |        +------------+
//                                    |        | Open in    |
//                                    +--#6--> | Preview    |
//                                             | (Mac only) |
//                                             +------------+
enum class PrintScanningContext {
  // Represents the moment the user presses ctrl-p/shift-ctrl-p or an equivalent
  // action that would lead to a system print dialog showing, before any such
  // dialog is shown.
  kBeforeSystemDialog = 0,

  // DEPRECATED
  // Represents the moment the user presses ctrl-p or an equivalent action
  // before any preview dialog is shown. This value is deprecated as policies no
  // longer apply print checks at the timing it used to represent. Since this
  // value was used in UMA, new code that hooks in a similar location should use
  // a different value.
  // kBeforePreview = 1,

  // Represents the moment the user has clicked "Print using system dialog",
  // before said dialog is shown and before the print job starts.
  kSystemPrintAfterPreview = 2,

  // Represents the moment the user has clicked "Print", before the print job
  // starts.
  kNormalPrintAfterPreview = 3,

  // Represents the code paths after the user has picked all printing settings
  // from either the print preview dialog or system dialog, right as the
  // document is about to be printed with a real print job. Also indicates what
  // kind of workflow was used to get those print settings.
  kSystemPrintBeforePrintDocument = 4,
  kNormalPrintBeforePrintDocument = 5,

#if BUILDFLAG(IS_MAC)
  // Represents the code paths after the user has clicked "Open PDF in Preview"
  // from the print preview dialog on Mac.
  kOpenPdfInPreview = 6,

  kMaxValue = kOpenPdfInPreview,
#else
  kMaxValue = kNormalPrintBeforePrintDocument,
#endif  // BUILDFLAG(IS_MAC)

};

// These functions take something to print (`print_data`) and scans it if the
// policy is enabled on a managed browser. It also passes on print metadata
// (e.g. `printer_name` or `scanning_data`) to content scans and `hides_preview`
// for the local ones. On receiving the verdict after the scan these functions
// calls `on_verdict` with true or false. In the non enterprise case where no
// scan is required, these functions directly calls `on_verdict` with true.
// These functions can return asynchronously.
void PrintIfAllowedByPolicy(scoped_refptr<base::RefCountedMemory> print_data,
                            content::WebContents* initiator,
                            std::string printer_name,
                            PrintScanningContext context,
                            base::OnceCallback<void(bool)> on_verdict,
                            base::OnceClosure hide_preview);
void PrintIfAllowedByPolicy(
    scoped_refptr<base::RefCountedMemory> print_data,
    content::WebContents* initiator,
    enterprise_connectors::ContentAnalysisDelegate::Data scanning_data,
    base::OnceCallback<void(bool)> on_verdict);

// Returns a `ContentAnalysisDelegate::Data` object with information about how
// content scanning should proceed, or nullopt if it shouldn't.
std::optional<enterprise_connectors::ContentAnalysisDelegate::Data>
GetPrintAnalysisData(content::WebContents* web_contents,
                     PrintScanningContext context);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PRINT_UTILS_H_

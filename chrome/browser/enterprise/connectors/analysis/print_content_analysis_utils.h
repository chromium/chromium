// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PRINT_CONTENT_ANALYSIS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PRINT_CONTENT_ANALYSIS_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace enterprise_connectors {

// Represents context for the kind of print workflow that needs to check if
// scanning should happen. This is used in conjunction with the
// `kEnableCloudScanAfterPreview` and `kEnableLocalScanAfterPreview` to control
// the timing at which scanning occurs.
//
//                 +-------------#3-------------+
//                 |                            V
//        +---------+        +--------+    +----------+        +-------+
// --#1-> | Preview | --#2-> | System | -> | Print    | --#4-> | Print |
//        | dialog  |        | dialog |    | document | --#5-> | job   |
//        +---------+        +--------+    +----------+        +-------+
//                               ^
// --#0--------------------------+
enum class PrintScanningContext {
  // Represents the moment the user presses ctrl-p/shift-ctrl-p or an equivalent
  // action before any preview/system dialog is shown.
  kBeforeSystemDialog = 0,
  kBeforePreview = 1,

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

  kMaxValue = kNormalPrintBeforePrintDocument,

  // TODO(b/281087582): Once cloud scanning support is added, add this option
  // for an extra Mac edge case for the extra "Open PDF in Preview" button on
  // the print preview dialog.
  // kMacOpenPdfInPreview = 6,

};

// These functions take something to print (`data`) and scans it if the policy
// is enabled on a managed browser. It also passes on print metadata (e.g.
// `printer_name` or `scanning_data`) to content scans and `hides_preview`
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
void PrintIfAllowedByPolicy(scoped_refptr<base::RefCountedMemory> print_data,
                            content::WebContents* initiator,
                            ContentAnalysisDelegate::Data scanning_data,
                            base::OnceCallback<void(bool)> on_verdict);

// Returns a `ContentAnalysisDelegate::Data` object with information about how
// content scanning should proceed, or nullopt if it shouldn't.
absl::optional<ContentAnalysisDelegate::Data> GetPrintAnalysisData(
    content::WebContents* web_contents,
    PrintScanningContext context);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PRINT_CONTENT_ANALYSIS_HANDLER_H_

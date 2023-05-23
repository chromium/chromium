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

// This function takes something to print (`data`) and scans it if the policy is
// enabled on a managed browser. It also passes on print metadata (e.g.
// `printer_name`) to content scans and `hides_preview` for the local ones. On
// receiving the verdict after the scan this function calls `on_verdict` with
// true or false. In the non enterprise case where no scan is required, this
// function directly calls `on_verdict` with true. This function can return
// asynchronously.
void PrintIfAllowedByPolicy(scoped_refptr<base::RefCountedMemory> data,
                            content::WebContents* initiator,
                            std::string printer_name,
                            base::OnceCallback<void(bool)> on_verdict,
                            base::OnceClosure hide_preview);

absl::optional<enterprise_connectors::ContentAnalysisDelegate::Data>
GetBeforePrintPreviewAnalysisData(content::WebContents* web_contents);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PRINT_CONTENT_ANALYSIS_HANDLER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "components/enterprise/common/files_scan_data.h"
#include "content/public/browser/content_browser_client.h"

namespace enterprise_data_protection {

// Apply appropriate data protection checks to pasted files.
// TODO(b/280449704): Remove this function from the header when
// crrev.com/c/5054488 lands and provides a cleaner interface.
void HandleExpandedPaths(
    std::unique_ptr<enterprise_connectors::FilesScanData> fsd,
    base::WeakPtr<content::WebContents> web_contents,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    enterprise_connectors::AnalysisConnector connector,
    std::vector<base::FilePath> paths,
    content::ContentBrowserClient::IsClipboardPasteContentAllowedCallback
        callback);

// Apply appropriate data protection checks to pasted text/images.
// TODO(b/280449704): Remove this function from the header when
// crrev.com/c/5054488 lands and provides a cleaner interface.
void HandleStringData(
    content::WebContents* web_contents,
    enterprise_connectors::ContentAnalysisDelegate::Data dialog_data,
    enterprise_connectors::AnalysisConnector connector,
    content::ContentBrowserClient::IsClipboardPasteContentAllowedCallback
        callback);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

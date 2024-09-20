// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

#include "base/feature_list.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/files_scan_data.h"
#include "content/public/browser/content_browser_client.h"

namespace enterprise_data_protection {

// This function checks if a paste is allowed to proceed according to the
// following policies:
// - DataLeakPreventionRulesList
// - OnBulkDataEntryEnterpriseConnector
// - DataControlsRules
//
// This function will always call `callback` after policies are evaluated with
// true if the paste is allowed to proceed and false if it is not. However, if
// policies indicate the paste action should receive a bypassable warning, then
// `callback` will only be called after the user makes the decision to bypass
// the warning or not. As such, callers should be careful not to bind data that
// could become dangling as `callback` is not guaranteed to run synchronously.
void PasteIfAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    content::ContentBrowserClient::IsClipboardPasteAllowedCallback callback);

// This function checks if data copied from a browser tab is allowed to be
// written to the OS clipboard according to the following policies:
// - CopyPreventionSettings
// - DataControlsRules
//
// If the copy is not allowed, `callback` is called with a replacement string
// that should instead be put into the OS clipboard.
void IsClipboardCopyAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback);

// This function replaces sub-fields in `data` depending internally tracked
// clipboard data that's been replaced due to the "DataControlsRules" policy.
// This should only be called for clipboard pastes within the same tab. If
// "DataControlsRules" is unset, this function does nothing.
void ReplaceSameTabClipboardDataIfRequiredByPolicy(
    ui::ClipboardSequenceNumberToken seqno,
    content::ClipboardPasteData& data);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

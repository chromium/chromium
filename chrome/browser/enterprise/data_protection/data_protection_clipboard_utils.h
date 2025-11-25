// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/files_scan_data.h"
#include "content/public/browser/content_browser_client.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_metadata.h"

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
    const ui::ClipboardMetadata& metadata,
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
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback);

#if BUILDFLAG(IS_ANDROID)
// This function checks if data being shared from a browser tab is allowed to
// be written to the OS clipboard according to the following policies:
// - DataControlsRules
//
// If the copy would not be allowed, `callback` is called with a replacement
// string that should instead be put into the OS clipboard.
void IsClipboardShareAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback);

// This function checks if the generic action is allowed to continue according
// to the following policies:
// - DataControlsRules
//
// If the copy would not be allowed, `callback` is called with a replacement
// string that should instead be put into the OS clipboard.
void IsClipboardGenericCopyActionAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data,
    content::ContentBrowserClient::IsClipboardCopyAllowedCallback callback);
#endif  //  BUILDFLAG(IS_ANDROID)

// This function replaces sub-fields in `data` depending internally tracked
// clipboard data that's been replaced due to the "DataControlsRules" policy.
// This should only be called for clipboard pastes within the same tab. If
// "DataControlsRules" is unset, this function does nothing.
void ReplaceSameTabClipboardDataIfRequiredByPolicy(
    ui::ClipboardSequenceNumberToken seqno,
    content::ClipboardPasteData& data);

// This function writes the given text to the clipboard. Wrapper over
// IsClipboardCopyAllowedByPolicy.
// Returns false if clipboard policy is not enforced, allowing the caller to use
// default clipboard write. Returns true if the function handles the clipboard
// write, potentially showing a dialog.
bool HandleWriteTextToClipboard(content::WebContents* web_contents,
                                ui::ClipboardBuffer clipboard_buffer,
                                const std::u16string_view& text);

// This function checks if drag and drop is allowed for the given source
// according to the DataControlsRules policy. Used to check if event is allowed
// synchronously without popup window.
bool DragAndDropForTextIsAllowed(content::WebContents* web_contents);

// Checks if the user is allowed to populate the find bar with
// the currently selected text in the given WebContents based on
// DataControlsRules policies. This is used to prevent potential data
// bypass through the find bar when copy/paste restrictions are in place.
// Returns true if populating the find bar is allowed, false otherwise.
bool CanPopulateFindBarFromSelection(content::WebContents* web_contents);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

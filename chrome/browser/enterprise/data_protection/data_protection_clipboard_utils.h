// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

#include <string>

#include "base/functional/callback.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/files_scan_data.h"
#include "content/public/browser/content_browser_client.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_metadata.h"

namespace content {
class RenderFrameHost;
class WebContents;
struct DropData;
}  // namespace content

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

// Returns true if `IsClipboardCopyAllowedByPolicy` needs to be called for the
// provided context. This is not required to be called before
// `IsClipboardCopyAllowedByPolicy` as it includes logic to return early in
// cases where policies aren't set or no restrictions are applied to the given
// context, this helper is provided as a convenience for caller code that wants
// to keep code synchronous when no enterprise restrictions are to be applied.
bool IsCopyPolicyCheckRequired(const content::ClipboardEndpoint& source,
                               const ui::ClipboardMetadata& metadata);

// This function checks if data dragged from a browser tab is allowed to be
// dragged to the OS according to the following policies:
// - DataControlsRules
//
// Returns true if the drag is allowed, false otherwise.
bool IsDragAllowedByPolicy(const content::ClipboardEndpoint& source,
                           const content::DropData& drop_data);

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

// Checks if the user is allowed to populate the find bar with
// the currently selected text in the given WebContents based on
// DataControlsRules policies. This is used to prevent potential data
// bypass through the find bar when copy/paste restrictions are in place.
// Returns true if populating the find bar is allowed, false otherwise.
bool CanPopulateFindBarFromSelection(content::WebContents* web_contents);

// Returns true if data copied from the find bar should be replaced before being
// put in the clipboard due to the "DataControlsRules" policy. If that is the
// case, the string put in `replacement` is what should instead by written to
// the clipboard.
bool ReplaceCopyFromFindBar(std::u16string_view selected_text,
                            content::WebContents* web_contents,
                            std::u16string* replacement);

// Checks if the given `web_contents` is allowed to receive replaced clipboard
// data, and returns it if so. This is used so `FindBarView` code doesn't always
// receive blocked pasted data in safe cases like searching a string in the same
// page it was copied from.
void ReplacePasteToFindBar(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::optional<std::u16string>)> callback);

// Checks if the user is allowed to use the "Search for..." context menu item
// in the given WebContents based on DataControlsRules policies.
// Returns true if search is allowed, false otherwise.
bool IsSearchWithAllowed(content::WebContents* web_contents);

// Checks if the user is allowed to use the "Search for..." context menu item
// in the given WebContents based on DataControlsRules policies.
// If the action is allowed (or reported/warned and bypassed),
// `on_allowed_callback` will be run.
void ShouldAllowSearchWith(content::WebContents* web_contents,
                           size_t selection_size,
                           base::OnceClosure on_allowed_callback);

// Copies `text` to the user's clipboard. This checks the Data Controls rules to
// ensure the copy is allowed.
void CopyTextToClipboard(content::RenderFrameHost* rfh,
                         const std::u16string& text);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_H_

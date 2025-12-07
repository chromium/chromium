// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_BROWSER_CONTEXT_DATA_H_
#define ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_BROWSER_CONTEXT_DATA_H_

#include <optional>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/unguessable_token.h"

// Functions for storing and retrieving `ScannerFeedbackInfo` on a browser
// context, keyed by an `base::UnguessableToken` ID. Allows access to a WebUI's
// `ScannerFeedbackInfo` from both the page handler (which stores the ID) as
// well as any request filters, provided the ID is encoded in the URL.

namespace content {
class BrowserContext;
}

namespace ash {

// Sets `feedback_info` keyed by `id` in `browser_context`.
// `feedback_info` can later be retrieved from the `Get` and `Take` functions
// below.
// Returns an object that, when destructed, also destructs `feedback_info` to
// prevent memory leaks.
base::ScopedClosureRunner SetScannerFeedbackInfoForBrowserContext(
    content::BrowserContext& browser_context,
    base::UnguessableToken id,
    ScannerFeedbackInfo feedback_info);

// Gets a reference to a `ScannerFeedbackInfo` keyed by `id` in
// `browser_context`, set by the `Set` function above.
// If no `ScannerFeedbackInfo` exists for `id`, returns nullptr.
ScannerFeedbackInfo* GetScannerFeedbackInfoForBrowserContext(
    content::BrowserContext& browser_context LIFETIME_BOUND,
    base::UnguessableToken id);

// Takes a `ScannerFeedbackInfo` keyed by `id` out of `browser_context`, set by
// the `Set` function above - removing it in the process.
// If no `ScannerFeedbackInfo` exists for `id`, returns nullopt.
std::optional<ScannerFeedbackInfo> TakeScannerFeedbackInfoForBrowserContext(
    content::BrowserContext& browser_context,
    base::UnguessableToken id);

}  // namespace ash

#endif  // ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_BROWSER_CONTEXT_DATA_H_

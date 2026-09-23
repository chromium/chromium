// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_
#define CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_

#include <string>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/feedback/public/feedback_source.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace chrome {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FeedbackDisabledDialogParentStatus)
enum class FeedbackDisabledDialogParentStatus {
  kDirectParent = 0,
  kFoundByFallback = 1,
  kNotFound = 2,
  kMaxValue = kNotFound,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:FeedbackDisabledDialogParentStatus)
#endif

// Returns whether the feedback page can be shown for the given `profile`.
// Callers can use this to hide or disable feedback UI entry points when
// appropriate.
bool CanShowFeedback(const Profile* profile);

// Displays the Feedback UI.
//
// `ShowFeedbackPage()` uses `bwi` to determine the URL of the current tab and
// the parent window. `bwi` should be nullptr if there are no currently open
// browser windows.
//
// If `CanShowFeedback()` is false for the target profile, on Desktop platforms
// (Win, Mac, Linux) a browser-modal dialog explaining that feedback is disabled
// is shown when `kFeedbackDisabledDialog` is enabled and a suitable parent
// browser window is available (either `bwi` or a fallback tabbed browser).
// Otherwise, this is a no-op (metrics are still recorded).
void ShowFeedbackPage(BrowserWindowInterface* bwi,
                      feedback::FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::DictValue autofill_metadata = base::DictValue(),
                      base::DictValue ai_metadata = base::DictValue());

// Same as above, for callers that specify `page_url` and `profile` directly
// instead of a `BrowserWindowInterface`.
void ShowFeedbackPage(const GURL& page_url,
                      Profile* profile,
                      feedback::FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::DictValue autofill_metadata = base::DictValue(),
                      base::DictValue ai_metadata = base::DictValue());

}  // namespace chrome

#endif  // CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_

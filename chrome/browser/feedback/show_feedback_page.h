// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_
#define CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/feedback/public/feedback_source.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace chrome {

// Returns whether the feedback page can be shown for the given `profile`.
bool CanShowFeedback(const Profile* profile);

// ShowFeedbackPage() uses `bwi` to determine the URL of the current tab.
// `bwi` should be NULL if there are no currently open browser windows.
//
// This is a no-op if `CanShowFeedback` is false for the profile corresponding
// to `bwi`. Callers should check `CanShowFeedback` before calling this
// function.
void ShowFeedbackPage(BrowserWindowInterface* bwi,
                      feedback::FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::DictValue autofill_metadata = base::DictValue(),
                      base::DictValue ai_metadata = base::DictValue());

// Displays the Feedback ui.
//
// This is a no-op if `CanShowFeedback` is false for the profile corresponding
// to `profile`. Callers should check `CanShowFeedback` before calling this
// function.
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

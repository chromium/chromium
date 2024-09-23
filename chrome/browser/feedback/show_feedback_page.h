// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_
#define CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/feedback/public/feedback_source.h"

class Browser;
class GURL;
class Profile;

namespace chrome {

// ShowFeedbackPage() uses |browser| to determine the URL of the current tab.
// |browser| should be NULL if there are no currently open browser windows.
void ShowFeedbackPage(const Browser* browser,
                      feedback::FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::Value::Dict autofill_metadata = base::Value::Dict(),
                      base::Value::Dict ai_metadata = base::Value::Dict());

// Displays the Feedback ui.
void ShowFeedbackPage(const GURL& page_url,
                      Profile* profile,
                      feedback::FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::Value::Dict autofill_metadata = base::Value::Dict(),
                      base::Value::Dict ai_metadata = base::Value::Dict());

}  // namespace chrome

#endif  // CHROME_BROWSER_FEEDBACK_SHOW_FEEDBACK_PAGE_H_

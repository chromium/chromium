// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_REPORT_UNSAFE_SITE_DIALOG_H_
#define CHROME_BROWSER_FEEDBACK_REPORT_UNSAFE_SITE_DIALOG_H_

class Browser;
class Profile;

namespace feedback {

// Contains static methods related to the report-unsafe-site dialog.
class ReportUnsafeSiteDialog {
 public:
  // Returns whether reporting unsafe sites is currently enabled.
  static bool IsEnabled(const Profile& profile);

  // Shows dialog in the passed-in browser.
  static void Show(Browser* browser);
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_REPORT_UNSAFE_SITE_DIALOG_H_

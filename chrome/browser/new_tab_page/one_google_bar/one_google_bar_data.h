// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_DATA_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_DATA_H_

#include <string>

// This struct contains all the data needed to inject a OneGoogleBar into a
// page.
struct OneGoogleBarData {
  OneGoogleBarData();
  OneGoogleBarData(const OneGoogleBarData&);
  OneGoogleBarData(OneGoogleBarData&&);
  ~OneGoogleBarData();

  OneGoogleBarData& operator=(const OneGoogleBarData&);
  OneGoogleBarData& operator=(OneGoogleBarData&&);

  // The main HTML for the bar itself.
  std::string bar_html;

  // "Page hooks" that need to be inserted at certain points in the page HTML.
  std::string in_head_script;
  std::string in_head_style;
  std::string after_bar_script;
  std::string end_of_body_html;
  std::string end_of_body_script;

  // User's language code returned by the server
  std::string language_code;
};

bool operator==(const OneGoogleBarData& lhs, const OneGoogleBarData& rhs);
bool operator!=(const OneGoogleBarData& lhs, const OneGoogleBarData& rhs);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_DATA_H_

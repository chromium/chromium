// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PROMPT_STATUS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PROMPT_STATUS_H_

// The status of the download location prompt.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.download
enum class DownloadPromptStatus {
  SHOW_INITIAL,     // Show the prompt because it hasn't been shown before.
  SHOW_PREFERENCE,  // Show the prompt because user indicated preference.
  DONT_SHOW,        // Don't show the prompt because user indicated preference.
  MAX_VALUE
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PROMPT_STATUS_H_

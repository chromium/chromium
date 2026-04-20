// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_CLIENT_TYPE_H_
#define CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_CLIENT_TYPE_H_

namespace context_sharing {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.tab_bottom_sheet
// Enum to identify different clients for the Tab Bottom Sheet.
enum class TabBottomSheetClientType {
  // Used for tests.
  kUnknown = 0,
  // Glic.
  kGlic = 1,
  // AIM.
  kContextualTasks = 2,
};

}  // namespace context_sharing

#endif  // CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_CLIENT_TYPE_H_

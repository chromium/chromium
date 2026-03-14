// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_LIST_TAB_REMOVED_REASON_H_
#define CHROME_BROWSER_TAB_LIST_TAB_REMOVED_REASON_H_

// Used to specify what will happen with the tab after it is removed.
enum class TabRemovedReason {
  // Tab will be deleted.
  kDeleted,

  // Tab got detached from a TabStrip and inserted into another TabStrip.
  kInsertedIntoOtherTabStrip,

  // Insert the WebContents into side panel.
  kInsertedIntoSidePanel,

  // Tab will be deleted and side panel will be expanded.
  kDeletedAndExpandSidePanel,
};

class TabRemoveReasonUtils {
 public:
  static bool WillDeleteTab(TabRemovedReason removed_reason);
  static bool WillDeleteWebContents(TabRemovedReason removed_reason);
};

#endif  // CHROME_BROWSER_TAB_LIST_TAB_REMOVED_REASON_H_

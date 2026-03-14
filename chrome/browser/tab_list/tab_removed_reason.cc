// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_list/tab_removed_reason.h"

bool TabRemoveReasonUtils::WillDeleteTab(TabRemovedReason removed_reason) {
  switch (removed_reason) {
    case TabRemovedReason::kDeleted:
    case TabRemovedReason::kDeletedAndExpandSidePanel:
    case TabRemovedReason::kInsertedIntoSidePanel:
      return true;
    case TabRemovedReason::kInsertedIntoOtherTabStrip:
      return false;
  }
}

bool TabRemoveReasonUtils::WillDeleteWebContents(
    TabRemovedReason removed_reason) {
  switch (removed_reason) {
    case TabRemovedReason::kDeleted:
    case TabRemovedReason::kDeletedAndExpandSidePanel:
      return true;
    case TabRemovedReason::kInsertedIntoSidePanel:
    case TabRemovedReason::kInsertedIntoOtherTabStrip:
      return false;
  }
}

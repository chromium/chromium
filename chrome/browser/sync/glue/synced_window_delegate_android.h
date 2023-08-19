// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegate.h"

class TabModel;

namespace sync_sessions {
class SyncedTabDelegate;
}  // namespace sync_sessions

namespace browser_sync {

class SyncedWindowDelegateAndroid : public sync_sessions::SyncedWindowDelegate {
 public:
  SyncedWindowDelegateAndroid(TabModel* tab_model, bool is_tabbed_activity);

  SyncedWindowDelegateAndroid(const SyncedWindowDelegateAndroid&) = delete;
  SyncedWindowDelegateAndroid& operator=(const SyncedWindowDelegateAndroid&) =
      delete;

  ~SyncedWindowDelegateAndroid() override;

  // sync_sessions::SyncedWindowDelegate implementation.
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const sync_sessions::SyncedTabDelegate* tab) const override;
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  const raw_ptr<TabModel> tab_model_;
  const bool is_tabbed_activity_ = false;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_

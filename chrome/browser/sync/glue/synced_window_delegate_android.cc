// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_window_delegate_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

using sync_sessions::SyncedTabDelegate;

namespace browser_sync {

// SyncedWindowDelegateAndroid implementations

SyncedWindowDelegateAndroid::SyncedWindowDelegateAndroid(
    TabModel* tab_model,
    bool is_tabbed_activity)
    : tab_model_(tab_model), is_tabbed_activity_(is_tabbed_activity) {}

SyncedWindowDelegateAndroid::~SyncedWindowDelegateAndroid() = default;

bool SyncedWindowDelegateAndroid::HasWindow() const {
  return !tab_model_->IsOffTheRecord();
}

SessionID SyncedWindowDelegateAndroid::GetSessionId() const {
  return tab_model_->GetSessionId();
}

int SyncedWindowDelegateAndroid::GetTabCount() const {
  return tab_model_->GetTabCount();
}

bool SyncedWindowDelegateAndroid::IsTypeNormal() const {
  return is_tabbed_activity_;
}

bool SyncedWindowDelegateAndroid::IsTypePopup() const {
  return false;
}

bool SyncedWindowDelegateAndroid::IsTabPinned(
    const SyncedTabDelegate* tab) const {
  return false;
}

SyncedTabDelegate* SyncedWindowDelegateAndroid::GetTabAt(int index) const {
  // After a restart, it is possible for the Tab to be null during startup.
  TabAndroid* tab = tab_model_->GetTabAt(index);
  return tab ? tab->GetSyncedTabDelegate() : nullptr;
}

SessionID SyncedWindowDelegateAndroid::GetTabIdAt(int index) const {
  SyncedTabDelegate* tab = GetTabAt(index);
  return tab ? tab->GetSessionId() : SessionID::InvalidValue();
}

bool SyncedWindowDelegateAndroid::IsSessionRestoreInProgress() const {
  return tab_model_->IsSessionRestoreInProgress();
}

bool SyncedWindowDelegateAndroid::ShouldSync() const {
  // We consider a window non-syncable if it contains at least one null tab.
  // This is sometimes the case during shutdown: on Android, when Custom Tab
  // windows are open as well as the browser itself when the browser is closed,
  // the window-closing transition exposes this weird state that, unless
  // filtered out, would cause tabs to be closed.
  for (int i = 0; i < tab_model_->GetTabCount(); ++i) {
    if (tab_model_->GetTabAt(i) == nullptr) {
      return false;
    }
  }
  return true;
}

}  // namespace browser_sync

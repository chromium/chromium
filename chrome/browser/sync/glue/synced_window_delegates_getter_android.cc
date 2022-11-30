// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sync_sessions/synced_window_delegate.h"

using sync_sessions::SyncedWindowDelegate;

namespace browser_sync {

SyncedWindowDelegatesGetterAndroid::SyncedWindowDelegatesGetterAndroid() =
    default;
SyncedWindowDelegatesGetterAndroid::~SyncedWindowDelegatesGetterAndroid() =
    default;

SyncedWindowDelegatesGetterAndroid::SyncedWindowDelegateMap
SyncedWindowDelegatesGetterAndroid::GetSyncedWindowDelegates() {
  SyncedWindowDelegateMap synced_window_delegates;
  for (const TabModel* model : TabModelList::models()) {
    synced_window_delegates[model->GetSyncedWindowDelegate()->GetSessionId()] =
        model->GetSyncedWindowDelegate();
  }
  return synced_window_delegates;
}

const SyncedWindowDelegate* SyncedWindowDelegatesGetterAndroid::FindById(
    SessionID session_id) {
  TabModel* tab_model = TabModelList::FindTabModelWithId(session_id);

  // In case we don't find the browser (e.g. for Developer Tools).
  return tab_model ? tab_model->GetSyncedWindowDelegate() : nullptr;
}

}  // namespace browser_sync

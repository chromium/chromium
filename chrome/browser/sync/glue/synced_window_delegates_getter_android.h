// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_

#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace sync_sessions {
class SyncedWindowDelegate;
}

namespace browser_sync {

// This class defines how to access SyncedWindowDelegates on Android.
class SyncedWindowDelegatesGetterAndroid
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  SyncedWindowDelegatesGetterAndroid();

  SyncedWindowDelegatesGetterAndroid(
      const SyncedWindowDelegatesGetterAndroid&) = delete;
  SyncedWindowDelegatesGetterAndroid& operator=(
      const SyncedWindowDelegatesGetterAndroid&) = delete;

  ~SyncedWindowDelegatesGetterAndroid() override;

  // SyncedWindowDelegatesGetter implementation
  SyncedWindowDelegateMap GetSyncedWindowDelegates() override;
  const sync_sessions::SyncedWindowDelegate* FindById(SessionID id) override;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

class Profile;

namespace sync_sessions {
class SyncedWindowDelegate;
}

// This class defines how to access SyncedWindowDelegates on Android.
class SyncedWindowDelegatesGetterAndroid
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  // `profile` must be non-null and must outlive `this`.
  explicit SyncedWindowDelegatesGetterAndroid(Profile* profile);

  SyncedWindowDelegatesGetterAndroid(
      const SyncedWindowDelegatesGetterAndroid&) = delete;
  SyncedWindowDelegatesGetterAndroid& operator=(
      const SyncedWindowDelegatesGetterAndroid&) = delete;

  ~SyncedWindowDelegatesGetterAndroid() override;

  // SyncedWindowDelegatesGetter implementation
  SyncedWindowDelegateMap GetSyncedWindowDelegates() override;
  const sync_sessions::SyncedWindowDelegate* FindById(SessionID id) override;

 private:
  const raw_ref<Profile> profile_;
};

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_

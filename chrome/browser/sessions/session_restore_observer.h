// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_OBSERVER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_OBSERVER_H_

#include <vector>

class Profile;
namespace sessions {
struct SessionWindow;
}  // namespace sessions

// Observer of events during session restore. This observer does not cover
// SessionRestoreImpl::RestoreForeignTab() which restores a single foreign tab.
class SessionRestoreObserver {
 public:
  // OnSessionRestoreStartedLoadingTabs() is called from session restore
  // prior to creating the first tab from session restore. Session restore may
  // do processing before this, and if no tabs are created (there was no
  // previous session, or perhaps the data was corrupt) this is not called.
  // OnSessionRestoreStartedLoadingTabs() is *not* called if another session
  // restore is triggered while waiting for a load to complete.
  virtual void OnSessionRestoreStartedLoadingTabs() {}

  // OnSessionRestoreFinishedLoadingTabs() is called once all the tabs created
  // by session restore have completed loading (or loading is canceled because
  // of memory pressure). This is called on the last session restore when
  // multiple concurrent session restores (on all profiles) occur.
  virtual void OnSessionRestoreFinishedLoadingTabs() {}

  // OnGotSession() is called right after windows are read from the last session
  // restore file. If windows are read by AppSessionService for app windows,
  // `for_app` is true. Otherwise, `for_app` is false. This function is used
  // for debug only.
  virtual void OnGotSession(
      Profile* profile,
      bool for_app,
      const std::vector<const sessions::SessionWindow*>& windows) {}

  // OnProfileSessionRestored() is called when a session restore for `profile`
  // finishes (either after creating the restored windows and starting tab
  // loading, or when cancelled), reporting how many normal and app windows were
  // restored. Both counts are 0 if there was nothing to restore or if the
  // restore was cancelled. At the point this is called, SessionRestore no
  // longer reports `profile` as restoring, though concurrent restores of other
  // profiles may still be in flight (see
  // SessionRestore::IsAnySessionCurrentlyRestoring()). Restores of a session
  // from another device do not call this.
  virtual void OnProfileSessionRestored(Profile* profile,
                                        int normal_windows,
                                        int app_windows) {}
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_OBSERVER_H_

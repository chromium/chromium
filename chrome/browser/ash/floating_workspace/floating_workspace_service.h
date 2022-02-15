// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace sync_sessions {
class OpenTabsUIDelegate;
class SessionSyncService;
struct SyncedSession;
}  // namespace sync_sessions

namespace ash {

class FloatingWorkspaceService : public KeyedService {
 public:
  static FloatingWorkspaceService* GetForProfile(Profile* profile);

  explicit FloatingWorkspaceService(Profile* profile);

  ~FloatingWorkspaceService() override;

  // Add subscription to foreign session changes.
  void SubscribeToForeignSessionUpdates();

  // Get and restore most recently used device browser session
  // remote or local.
  void RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  void TryRestoreMostRecentlyUsedSession();

 private:
  const sync_sessions::SyncedSession* GetMostRecentlyUsedRemoteSession();

  const sync_sessions::SyncedSession* GetLocalSession();

  void RestoreForeignSessionWindows(
      const sync_sessions::SyncedSession* session);

  void RestoreLocalSessionWindows();

  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  Profile* const profile_;

  sync_sessions::SessionSyncService* const session_sync_service_;

  base::CallbackListSubscription foreign_session_updated_subscription_;

  // Flag determine if we should run the restore.
  bool should_run_restore_ = true;

  // Time when the service is initialized during login.
  const base::Time initialized_timestamp_;

  base::WeakPtrFactory<FloatingWorkspaceService> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_SERVICE_H_

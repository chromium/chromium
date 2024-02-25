// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "extensions/browser/extension_registry_observer.h"

class AppSyncUIStateObserver;
class Profile;

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace syncer {
class SyncService;
}  // namespace syncer

// AppSyncUIState watches app sync and installation and change its state
// accordingly. Its status is for UI display only. It only watches for new
// normal user profile (i.e. it does not watch for guest profile or exsiting
// user profile) and lasts for at the most 1 minute.
class AppSyncUIState : public KeyedService,
                       public syncer::SyncServiceObserver,
                       public extensions::ExtensionRegistryObserver {
 public:
  enum Status {
    STATUS_NORMAL,
    STATUS_SYNCING,    // Syncing apps or installing synced apps.
    STATUS_TIMED_OUT,  // Timed out when waiting for sync to finish.
  };

  // Returns the instance for the given |profile|. It's a convenience wrapper
  // of AppSyncUIStateFactory::GetForProfile. Note this function returns
  // NULL if ShouldObserveAppSyncForProfile returns false for |profile|.
  static AppSyncUIState* Get(Profile* profile);

  // Returns true if |profile| should be watched for app syncing.
  static bool ShouldObserveAppSyncForProfile(Profile* profile);

  explicit AppSyncUIState(Profile* profile);

  AppSyncUIState(const AppSyncUIState&) = delete;
  AppSyncUIState& operator=(const AppSyncUIState&) = delete;

  ~AppSyncUIState() override;

  void AddObserver(AppSyncUIStateObserver* observer);
  void RemoveObserver(AppSyncUIStateObserver* observer);

  Status status() const { return status_; }

  // KeyedService implementation.
  void Shutdown() override;

 private:
  void StartObserving();
  void StopObserving();

  void SetStatus(Status status);

  // Checks and sets app sync status. If sync has not setup, do nothing. If sync
  // is completed and there is no pending synced extension install, sets
  // STATUS_SYNCING. Otherwise, sets STATUS_NORMAL.
  void CheckAppSync();

  // Invoked when |max_syncing_status_timer_| fires.
  void OnMaxSyncingTimer();

  // syncer::SyncServiceObserver overrides:
  void OnStateChanged(syncer::SyncService* sync) override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

  raw_ptr<Profile> profile_;
  raw_ptr<syncer::SyncService> sync_service_;

  // Timer to limit how much time STATUS_SYNCING is allowed.
  base::OneShotTimer max_syncing_status_timer_;

  Status status_;
  base::ObserverList<AppSyncUIStateObserver> observers_;

  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_H_

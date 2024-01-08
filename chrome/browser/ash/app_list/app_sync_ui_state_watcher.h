// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_sync_ui_state_observer.h"

class AppListModelUpdater;
class AppSyncUIState;
class Profile;

// AppSyncUIStateWatcher updates AppListModel status when AppSyncUIState
// of the given profile changes.
class AppSyncUIStateWatcher : public AppSyncUIStateObserver {
 public:
  AppSyncUIStateWatcher(Profile* profile, AppListModelUpdater* model_updater);

  AppSyncUIStateWatcher(const AppSyncUIStateWatcher&) = delete;
  AppSyncUIStateWatcher& operator=(const AppSyncUIStateWatcher&) = delete;

  ~AppSyncUIStateWatcher() override;

 private:
  // AppSyncUIStateObserver overrides:
  void OnAppSyncUIStatusChanged() override;

  raw_ptr<AppSyncUIState> app_sync_ui_state_;
  // Owned by AppListSyncableService
  raw_ptr<AppListModelUpdater> model_updater_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_

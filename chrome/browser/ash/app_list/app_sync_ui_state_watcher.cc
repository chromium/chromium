// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_sync_ui_state_watcher.h"

#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_sync_ui_state.h"

AppSyncUIStateWatcher::AppSyncUIStateWatcher(Profile* profile,
                                             AppListModelUpdater* model_updater)
    : app_sync_ui_state_(AppSyncUIState::Get(profile)),
      model_updater_(model_updater) {
  if (app_sync_ui_state_) {
    app_sync_ui_state_->AddObserver(this);
    OnAppSyncUIStatusChanged();
  }
}

AppSyncUIStateWatcher::~AppSyncUIStateWatcher() {
  if (app_sync_ui_state_)
    app_sync_ui_state_->RemoveObserver(this);
}

void AppSyncUIStateWatcher::OnAppSyncUIStatusChanged() {
  if (app_sync_ui_state_->status() == AppSyncUIState::STATUS_SYNCING)
    model_updater_->SetStatus(ash::AppListModelStatus::kStatusSyncing);
  else
    model_updater_->SetStatus(ash::AppListModelStatus::kStatusNormal);
}

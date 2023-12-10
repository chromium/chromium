// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_test_support.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync_sessions/session_sync_service.h"
#include "url/gurl.h"

MockOpenTabsUIDelegate::MockOpenTabsUIDelegate() = default;

MockOpenTabsUIDelegate::~MockOpenTabsUIDelegate() = default;

MockSessionSyncService::MockSessionSyncService() = default;

MockSessionSyncService::~MockSessionSyncService() = default;

syncer::GlobalIdMapper* MockSessionSyncService::GetGlobalIdMapper() const {
  return nullptr;
}

MockOpenTabsUIDelegate* MockSessionSyncService::GetOpenTabsUIDelegate() {
  return &mock_open_tabs_ui_delegate_;
}

base::CallbackListSubscription
MockSessionSyncService::SubscribeToForeignSessionsChanged(
    const base::RepeatingClosure& cb) {
  return subscriber_list_.Add(cb);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
MockSessionSyncService::GetControllerDelegate() {
  return nullptr;
}

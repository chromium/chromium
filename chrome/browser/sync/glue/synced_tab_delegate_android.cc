// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
#include "chrome/browser/sync/glue/web_contents_state_synced_tab_delegate.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace browser_sync {

SyncedTabDelegateAndroid::SyncedTabDelegateAndroid(TabAndroid* tab_android)
    : tab_android_(tab_android) {}

SyncedTabDelegateAndroid::~SyncedTabDelegateAndroid() = default;

SessionID SyncedTabDelegateAndroid::GetWindowId() const {
  return tab_android_->window_id();
}

SessionID SyncedTabDelegateAndroid::GetSessionId() const {
  return SessionIdFromAndroidId(tab_android_->GetAndroidId());
}

bool SyncedTabDelegateAndroid::IsPlaceholderTab() const {
  return web_contents() == nullptr;
}

// This function creates a synced tab delegate for a tab that was previously
// known as a placeholder tab. After the new synced tab delegate has been
// created, the associated tab should no longer be seen as a placeholder tab.
std::unique_ptr<sync_sessions::SyncedTabDelegate>
SyncedTabDelegateAndroid::CreatePlaceholderTabSyncedTabDelegate() {
  std::unique_ptr<WebContentsStateByteBuffer> web_contents_byte_buffer =
      tab_android_->GetWebContentsByteBuffer();

  // If the web contents is null return a nullptr to the callback to be handled
  // in local_session_event_handler.
  if (!web_contents_byte_buffer) {
    return nullptr;
  }

  return browser_sync::WebContentsStateSyncedTabDelegate::Create(
      tab_android_, std::move(web_contents_byte_buffer));
}

void SyncedTabDelegateAndroid::SetWebContents(
    content::WebContents* web_contents) {
  TabContentsSyncedTabDelegate::SetWebContents(web_contents);
}

void SyncedTabDelegateAndroid::ResetWebContents() {
  TabContentsSyncedTabDelegate::SetWebContents(nullptr);
}

SessionID SyncedTabDelegateAndroid::SessionIdFromAndroidId(int android_tab_id) {
  // Increment with 1 since SessionID considers zero as invalid value, whereas
  // Android IDs start at 0.
  // TODO(crbug.com/853731): Returning SessionID instances that haven't been
  // generated with SessionID::NewUnique() is problematic or at least hard to
  // reason about, due to possible conflicts in case they were put together or
  // compared with regular SessionID instances. We should either migrate this
  // whole class hierarchy away from type SessionID, or alternative unify the ID
  // generation between Android and SessionIDs.
  return SessionID::FromSerializedValue(1 + android_tab_id);
}

}  // namespace browser_sync

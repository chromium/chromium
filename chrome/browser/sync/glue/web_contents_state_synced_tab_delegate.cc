// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/web_contents_state_synced_tab_delegate.h"

namespace browser_sync {

WebContentsStateSyncedTabDelegate::WebContentsStateSyncedTabDelegate(
    TabAndroid* tab_android,
    std::unique_ptr<WebContentsStateByteBuffer> web_contents_byte_buffer)
    : tab_android_(tab_android),
      web_contents_buffer_(std::move(web_contents_byte_buffer)) {
  web_contents_ = WebContentsState::RestoreContentsFromByteBuffer(
      web_contents_buffer_.get(), /*initially_hidden=*/true,
      /*no_renderer=*/true);
  TabContentsSyncedTabDelegate::SetWebContents(web_contents_.get());
}

WebContentsStateSyncedTabDelegate::~WebContentsStateSyncedTabDelegate() =
    default;

SessionID WebContentsStateSyncedTabDelegate::GetWindowId() const {
  return tab_android_->window_id();
}

SessionID WebContentsStateSyncedTabDelegate::GetSessionId() const {
  return browser_sync::SyncedTabDelegateAndroid::SessionIdFromAndroidId(
      tab_android_->GetAndroidId());
}

bool WebContentsStateSyncedTabDelegate::IsPlaceholderTab() const {
  // We will check if the tab is a placeholder tab depending on if there is a
  // valid web contents or not. If there are no valid web contents after
  // attempted restoration then it will be caught in checks downstream at
  // local_session_event_handler.cc.
  return web_contents() == nullptr;
}

std::unique_ptr<sync_sessions::SyncedTabDelegate>
WebContentsStateSyncedTabDelegate::CreatePlaceholderTabSyncedTabDelegate() {
  NOTREACHED() << "CreatePlaceholderTabSyncedTabDelegate should be called only "
                  "via the SyncedTabDelegateAndroid implementation.";
  return nullptr;
}

}  // namespace browser_sync

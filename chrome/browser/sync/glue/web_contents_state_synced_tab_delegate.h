// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_WEB_CONTENTS_STATE_SYNCED_TAB_DELEGATE_H_
#define CHROME_BROWSER_SYNC_GLUE_WEB_CONTENTS_STATE_SYNCED_TAB_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android_data_provider.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/sessions/core/session_id.h"

namespace browser_sync {

// An implementation of TabContentsSyncedTabDelegate, with overridden
// functions specific to Android, as a tab can exist without a web contents
// being present (placeholder tab). This is a temporary representation of a
// placeholder tab's web contents, which is done by creating a renderless web
// contents using the tab's web contents state. The web contents state is
// mmapped so it is not expensive to restore the web contents from, and allows
// the SyncedTabDelegateAndroid::ReadPlaceholderTabSnapshotIfItShouldSync to be
// performed synchronously as no disk read is required.
class WebContentsStateSyncedTabDelegate : public TabContentsSyncedTabDelegate {
 public:
  WebContentsStateSyncedTabDelegate(const WebContentsStateSyncedTabDelegate&) =
      delete;
  WebContentsStateSyncedTabDelegate& operator=(
      const WebContentsStateSyncedTabDelegate&) = delete;

  ~WebContentsStateSyncedTabDelegate() override;

  static std::unique_ptr<WebContentsStateSyncedTabDelegate> Create(
      TabAndroidDataProvider* tab_android_data_provider,
      std::unique_ptr<WebContentsStateByteBuffer> web_contents_byte_buffer);

  // SyncedTabDelegate android specific overrides:
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsPlaceholderTab() const override;
  std::unique_ptr<sync_sessions::SyncedTabDelegate>
  ReadPlaceholderTabSnapshotIfItShouldSync(
      sync_sessions::SyncSessionsClient* sessions_client) override;

  // Check if the synced tab delegate has a valid web contents.
  bool HasWebContents() const;

 private:
  WebContentsStateSyncedTabDelegate(
      TabAndroidDataProvider* tab_android_data_provider,
      std::unique_ptr<WebContentsStateByteBuffer> web_contents_byte_buffer);

  raw_ptr<TabAndroidDataProvider> tab_android_data_provider_;
  std::unique_ptr<WebContentsStateByteBuffer> web_contents_buffer_;
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_WEB_CONTENTS_STATE_SYNCED_TAB_DELEGATE_H_

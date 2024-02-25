// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_RECENT_TAB_HELPER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/snapshot_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

namespace offline_pages {

// Attaches to every WebContent shown in a tab. Waits until the WebContent is
// loaded to proper degree and then makes a snapshot of the page. Removes the
// oldest snapshot in the 'ring buffer'. As a result, there is always up to N
// snapshots of recent pages on the device.
class RecentTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RecentTabHelper>,
      public SnapshotController::Client {
 public:
  RecentTabHelper(const RecentTabHelper&) = delete;
  RecentTabHelper& operator=(const RecentTabHelper&) = delete;

  ~RecentTabHelper() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Notifies that the tab of the associated WebContents will (most probably) be
  // closed. This call is expected to always happen before the one to WasHidden.
  void WillCloseTab();

  // SnapshotController::Client
  void StartSnapshot() override;

  // Delegate that is used by RecentTabHelper to get external dependencies.
  // Default implementation lives in .cc file, while tests provide an override.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<OfflinePageArchiver> CreatePageArchiver(
        content::WebContents* web_contents) = 0;
    // There is no expectations that tab_id is always present.
    virtual bool GetTabId(content::WebContents* web_contents, int* tab_id) = 0;
    virtual bool IsLowEndDevice() = 0;
    virtual bool IsCustomTab(content::WebContents* web_contents) = 0;
  };
  void SetDelegate(std::unique_ptr<RecentTabHelper::Delegate> delegate);

  // Creates a request to download the current page with a properly filled
  // |client_id| and valid |request_id| issued by RequestCoordinator from a
  // suspended request. This method might be called multiple times for the same
  // page at any point after its navigation commits. There are some important
  // points about how requests are handled:
  // a) While there is an ongoing request, new requests are ignored (no
  //    overlapping snapshots).
  // b) The page has to be sufficiently loaded to be considerer of minimum
  //    quality for the request to be started immediately.
  // c) Any calls made before the page is considered to have minimal quality
  //    will be scheduled to be executed once that happens. The scheduled
  //    request is considered "ongoing" for a) purposes.
  // d) If the save operation is successful the dormant request with
  //    RequestCoordinator is canceled; otherwise it is resumed. This logic is
  //    robust to crashes.
  // e) At the moment the page reaches high quality, if there was a successful
  //    snapshot saved at a lower quality then a new snapshot is automatically
  //    requested to replace it.
  // Note #1: Page quality is determined by SnapshotController and is based on
  // its assessment of "how much loaded" it is.
  // Note #2: Currently this method only accepts download requests from the
  // downloads namespace.
  void ObserveAndDownloadCurrentPage(const ClientId& client_id,
                                     int64_t request_id,
                                     const std::string& origin);

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentTabHelperFencedFrameTest,
                           FencedFrameDoesNotChangePageQuality);

  struct SnapshotProgressInfo;

  explicit RecentTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<RecentTabHelper>;

  void WebContentsWasHidden();
  void WebContentsWasShown();

  bool EnsureInitialized();
  void ContinueSnapshotWithIdsToPurge(SnapshotProgressInfo* snapshot_info,
                                      const std::vector<int64_t>& page_ids);
  void ContinueSnapshotAfterPurge(SnapshotProgressInfo* snapshot_info,
                                  OfflinePageModel::DeletePageResult result);
  void SavePageCallback(SnapshotProgressInfo* snapshot_info,
                        OfflinePageModel::SavePageResult result,
                        int64_t offline_id);
  void ReportSnapshotCompleted(SnapshotProgressInfo* snapshot_info,
                               bool success);
  void ReportDownloadStatusToRequestCoordinator(
      SnapshotProgressInfo* snapshot_info,
      bool cancel_background_request);
  ClientId GetRecentPagesClientId() const;
  void SaveSnapshotForDownloads(bool replace_latest);
  void CancelInFlightSnapshots();

  // Page model is a service, no ownership. Can be null - for example, in
  // case when tab is in incognito profile.
  raw_ptr<OfflinePageModel> page_model_ = nullptr;

  // If false, never make snapshots off the attached WebContents.
  // Not page-specific.
  bool snapshots_enabled_ = false;

  // Snapshot progress information for an ongoing snapshot requested by
  // downloads. Null if there's no ongoing request.
  std::unique_ptr<SnapshotProgressInfo> downloads_ongoing_snapshot_info_;

  // This is set to true if the ongoing snapshot for downloads is waiting on the
  // page to reach a minimal quality level to start.
  bool downloads_snapshot_on_hold_ = false;

  // Snapshot information for the last successful snapshot requested by
  // downloads. Null if no successful one has ever completed for the current
  // page.
  std::unique_ptr<SnapshotProgressInfo> downloads_latest_saved_snapshot_info_;

  // Snapshot progress information for a last_n triggered request. Null if
  // last_n is not currently capturing the current page. It is cleared upon non
  // ignored navigations.
  std::unique_ptr<SnapshotProgressInfo> last_n_ongoing_snapshot_info_;

  // Snapshot information for the last successful snapshot requested by
  // last_n for the currently loaded page. Null if no successful one has ever
  // completed for the current page. It is cleared when the referenced snapshot
  // is about to be deleted.
  std::unique_ptr<SnapshotProgressInfo> last_n_latest_saved_snapshot_info_;

  // If empty, the tab does not have AndroidId and can not capture pages.
  std::string tab_id_;

  // Monitors page loads and starts snapshots when a download request exist. It
  // is also used as an initialization flag for EnsureInitialized() to be run
  // only once.
  std::unique_ptr<SnapshotController> snapshot_controller_;

  std::unique_ptr<Delegate> delegate_;

  // Set at each navigation to control if last_n should save snapshots of the
  // current page being loaded.
  bool last_n_listen_to_tab_hidden_ = false;

  // Set to true when the tab containing the associated WebContents is in the
  // process of being closed.
  bool tab_is_closing_ = false;

  base::WeakPtrFactory<RecentTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_RECENT_TAB_HELPER_H_

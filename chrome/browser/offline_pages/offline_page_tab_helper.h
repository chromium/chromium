// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/common/mhtml_page_notifier.mojom.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/mojom/loader/mhtml_load_result.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace offline_pages {

struct OfflinePageItem;
class PrefetchService;

// This enum is used for UMA reporting. It contains all possible trusted states
// of the offline page.
// NOTE: because this is used for UMA reporting, these values should not be
// changed or reused; new values should be ended immediately before the MAX
// value. Make sure to update the histogram enum (OfflinePageTrustedState in
// enums.xml) accordingly.
enum class OfflinePageTrustedState {
  // Trusted because the archive file is in internal directory.
  TRUSTED_AS_IN_INTERNAL_DIR,
  // Trusted because the archive file is in public directory without
  // modification.
  TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR,
  // No trusted because the archive file is in public directory and it is
  // modified.
  UNTRUSTED,
  TRUSTED_STATE_MAX
};

// Per-tab class that monitors the navigations and stores the necessary info
// to facilitate the synchronous access to offline information.
class OfflinePageTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OfflinePageTabHelper>,
      public offline_pages::mojom::MhtmlPageNotifier {
 public:
  ~OfflinePageTabHelper() override;

  // MhtmlPageNotifier overrides.
  void NotifyMhtmlPageLoadAttempted(blink::mojom::MHTMLLoadResult result,
                                    const GURL& main_frame_url,
                                    base::Time date) override;

  void SetOfflinePage(const OfflinePageItem& offline_page,
                      const OfflinePageHeader& offline_header,
                      OfflinePageTrustedState trusted_state,
                      bool is_offline_preview);

  void ClearOfflinePage();

  OfflinePageItem* offline_page() { return offline_info_.offline_page.get(); }

  const OfflinePageHeader& offline_header() const {
    return offline_info_.offline_header;
  }

  OfflinePageTrustedState trusted_state() const {
    return offline_info_.trusted_state;
  }

  // Returns whether a trusted offline page is being displayed.
  bool IsShowingTrustedOfflinePage() const;

  // Returns nullptr if the page is not an offline preview. Returns the
  // OfflinePageItem related to the page if the page is an offline preview.
  const OfflinePageItem* GetOfflinePreviewItem() const;

  // Returns provisional offline page since actual navigation does not happen
  // during unit tests.
  const OfflinePageItem* GetOfflinePageForTest() const;

  // True if an offline page is loading, but has not committed.
  bool IsLoadingOfflinePage() const;

  // Returns trusted state of provisional offline page.
  OfflinePageTrustedState GetTrustedStateForTest() const;

  // Sets the target frame, useful for unit testing the MhtmlPageNotifier
  // interface.
  void SetCurrentTargetFrameForTest(
      content::RenderFrameHost* render_frame_host);

  // Helper function which normally should only be called by
  // OfflinePageUtils::ScheduleDownload to do the work. This is because we need
  // to ensure |web_contents| is still valid after returning from the
  // asynchronous call of duplicate checking function. The lifetime of
  // OfflinePageTabHelper instance is tied with the associated |web_contents|
  // and thus the callback will be automatically invalidated if |web_contents|
  // is gone.
  void ScheduleDownloadHelper(content::WebContents* web_contents,
                              const std::string& name_space,
                              const GURL& url,
                              OfflinePageUtils::DownloadUIActionFlags ui_action,
                              const std::string& request_origin);

 private:
  friend class content::WebContentsUserData<OfflinePageTabHelper>;

  // Contains the info about the offline page being loaded.
  struct LoadedOfflinePageInfo {
    LoadedOfflinePageInfo();
    ~LoadedOfflinePageInfo();

    // Constructs a valid but untrusted LoadedOfflinePageInfo with |url| as the
    // online URL.
    static LoadedOfflinePageInfo MakeUntrusted();

    LoadedOfflinePageInfo& operator=(LoadedOfflinePageInfo&& other);
    LoadedOfflinePageInfo(LoadedOfflinePageInfo&& other);

    // The cached copy of OfflinePageItem. Note that if |is_trusted| is false,
    // offline_page may contain information derived from the MHTML itself and
    // should be exposed to the user as untrusted.
    std::unique_ptr<OfflinePageItem> offline_page;

    // The offline header that is provided when offline page is loaded.
    OfflinePageHeader offline_header;

    // The trusted state of the page.
    OfflinePageTrustedState trusted_state;

    // Whether the page is an offline preview. Offline page previews are shown
    // when a user's effective connection type is prohibitively slow.
    bool is_showing_offline_preview = false;

    // Returns true if this contains an offline page.  When constructed,
    // LoadedOfflinePageInfo objects are invalid until filled with an offline
    // page.
    bool IsValid() const;

    void Clear();
  };

  explicit OfflinePageTabHelper(content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Finalize the offline info when the navigation is done.
  void FinalizeOfflineInfo(content::NavigationHandle* navigation_handle);

  void ReportOfflinePageMetrics();

  // Report the metrics essential to PrefetchService.
  void ReportPrefetchMetrics(content::NavigationHandle* navigation_handle);

  // Reload the URL in order to fetch the offline page on certain net errors.
  void TryLoadingOfflinePageOnNetError(
      content::NavigationHandle* navigation_handle);

  // Creates an offline info with an invalid offline ID and the given URL.
  LoadedOfflinePageInfo MakeUntrustedOfflineInfo(const GURL& url);

  void SelectPagesForURLDone(const std::vector<OfflinePageItem>& offline_pages);

  void DuplicateCheckDoneForScheduleDownload(
      content::WebContents* web_contents,
      const std::string& name_space,
      const GURL& url,
      OfflinePageUtils::DownloadUIActionFlags ui_action,
      const std::string& request_origin,
      OfflinePageUtils::DuplicateCheckResult result);
  void DoDownloadPageLater(content::WebContents* web_contents,
                           const std::string& name_space,
                           const GURL& url,
                           OfflinePageUtils::DownloadUIActionFlags ui_action,
                           const std::string& request_origin);

  // The provisional info about the offline page being loaded. This is set when
  // the offline interceptor decides to serve the offline page and it will be
  // moved to |offline_info_| once the navigation is committed without error.
  LoadedOfflinePageInfo provisional_offline_info_;

  // The info about offline page being loaded. This is set from
  // |provisional_offline_info_| when the navigation is committed without error.
  // This can be used to by the Tab to synchronously ask about the offline
  // info.
  LoadedOfflinePageInfo offline_info_;

  bool reloading_url_on_net_error_ = false;

  // Service, outlives this object.
  PrefetchService* prefetch_service_ = nullptr;

  // TODO(crbug.com/827215): We only really want interface messages for the main
  // frame but this is not easily done with the current helper classes.
  content::WebContentsFrameReceiverSet<mojom::MhtmlPageNotifier>
      mhtml_page_notifier_receivers_;

  base::WeakPtrFactory<OfflinePageTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_TAB_HELPER_H_

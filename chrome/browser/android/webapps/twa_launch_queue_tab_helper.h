// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_TAB_HELPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/webapps/twa_launch_navigation_handle_user_data.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {
class WebContents;
}

namespace webapps {

class LaunchQueue;

// Tab helper that manages the lifecycle of Trusted Web Activity (TWA) launches
// and bridges them to the C++ LaunchQueue.
//
// This class orchestrates a state machine to handle the asynchronous nature of
// TWA launches, where the launch intent and Digital Asset Links (DAL)
// verification happen in Java, while the navigation and document creation
// happen in C++.
//
// The state machine tracks launches through three main stages:
// 1. **Pending**: A launch has been prepared in Java (via JNI
// `PrepareForLaunch`)
//    with a unique token, but the corresponding navigation has not yet started.
// 2. **Active**: The navigation for the launch has started
// (`DidStartNavigation`
//    matched by token) and is currently in progress. The launch data is
//    attached to the `NavigationHandle`'s user data.
// 3. **Committed/Enqueued**:
//    - If the navigation commits and DAL verification is already complete and
//      successful, the launch parameters are immediately enqueued to the
//      `LaunchQueue`.
//    - If the navigation commits but DAL verification is still pending, the
//      launch is stashed in `committed_launches_` until verification completes
//      (notified via JNI `OnLaunchVerified`).
//    - If the navigation commits but is out-of-scope, or if a new document
//      navigation commits, the launch is discarded to prevent cross-origin
//      leaks.
//
// It also handles edge cases like:
// - *Non-navigating launches* (focus-existing): Enqueued immediately if the
//   current page is same-origin and already verified.
// - *Speculative launches*: Matched by URL fallback if the navigation was
//   pre-warmed and started before the launch token was available.
class TwaLaunchQueueTabHelper
    : public content::WebContentsUserData<TwaLaunchQueueTabHelper>,
      public content::WebContentsObserver {
 public:
  explicit TwaLaunchQueueTabHelper(content::WebContents* contents);
  TwaLaunchQueueTabHelper(const TwaLaunchQueueTabHelper&) = delete;
  TwaLaunchQueueTabHelper& operator=(const TwaLaunchQueueTabHelper&) = delete;
  ~TwaLaunchQueueTabHelper() override;

  LaunchQueue& EnsureLaunchQueue();

  void PrepareForLaunch(int64_t launch_token,
                        LaunchParams launch_params,
                        bool has_speculative_navigation);
  void OnLaunchVerified(int64_t launch_token, bool success);
  void EnqueueNonNavigating(LaunchParams launch_params);

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  void FlushLaunchQueueForTesting() const;

 private:
  friend class content::WebContentsUserData<TwaLaunchQueueTabHelper>;

  // Use unique_ptr for lazy instantiation as most browser tabs have no need to
  // incur this memory overhead.
  std::unique_ptr<LaunchQueue> launch_queue_;

  // Stores launch parameters and DAL verification status for a launch that has
  // been prepared in Java but has not yet matched a C++ navigation.
  struct PendingLaunch {
    LaunchParams params;
    DigitalAssetLinksVerificationStatus status =
        DigitalAssetLinksVerificationStatus::kPending;
    // True if this launch has an associated speculative navigation that was
    // started beforehand (e.g. via mayLaunchUrl). We do not expect a new
    // navigation to start, and will try to match the speculative one.
    bool has_speculative_navigation = false;
  };

  // Stores launch parameters and the RFH ID for a launch whose navigation has
  // committed but DAL verification is still pending.
  struct CommittedLaunch {
    LaunchParams params;
    content::GlobalRenderFrameHostId rfh_id;
  };

  // Maps launch tokens to their pending launch data. Holds launches before they
  // match a navigation.
  absl::flat_hash_map<int64_t, PendingLaunch> pending_launches_;

  // Maps launch tokens to their active navigation handles. Holds launches while
  // their navigation is active (started but not finished).
  absl::flat_hash_map<int64_t, raw_ptr<content::NavigationHandle>>
      active_launches_;

  // Maps launch tokens to their committed launch data. Holds launches that have
  // committed but are waiting for DAL verification to complete.
  absl::flat_hash_map<int64_t, CommittedLaunch> committed_launches_;

  base::WeakPtrFactory<TwaLaunchQueueTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_QUEUE_TAB_HELPER_H_

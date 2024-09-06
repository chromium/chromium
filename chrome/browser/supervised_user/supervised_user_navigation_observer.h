// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/common/supervised_user_commands.mojom.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace supervised_user {
class SupervisedUserService;
class SupervisedUserInterstitial;
}  // namespace supervised_user

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

using OnInterstitialResultCallback = base::RepeatingCallback<
    void(SupervisedUserNavigationThrottle::CallbackActions, bool, bool)>;

class SupervisedUserNavigationObserver
    : public content::WebContentsUserData<SupervisedUserNavigationObserver>,
      public content::WebContentsObserver,
      public SupervisedUserServiceObserver,
      public supervised_user::mojom::SupervisedUserCommands {
 public:
  SupervisedUserNavigationObserver(const SupervisedUserNavigationObserver&) =
      delete;
  SupervisedUserNavigationObserver& operator=(
      const SupervisedUserNavigationObserver&) = delete;

  ~SupervisedUserNavigationObserver() override;

  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>&
  blocked_navigations() const {
    return blocked_navigations_;
  }

  static void BindSupervisedUserCommands(
      mojo::PendingAssociatedReceiver<
          supervised_user::mojom::SupervisedUserCommands> receiver,
      content::RenderFrameHost* rfh);

  // Called when a network request to |url| is blocked.
  static void OnRequestBlocked(content::WebContents* web_contents,
                               const GURL& url,
                               supervised_user::FilteringBehaviorReason reason,
                               int64_t navigation_id,
                               content::FrameTreeNodeId frame_id,
                               const OnInterstitialResultCallback& callback);

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

  // Called when interstitial error page is no longer being shown in the main
  // frame.
  void OnInterstitialDone(content::FrameTreeNodeId frame_id);

  const std::map<content::FrameTreeNodeId,
                 std::unique_ptr<supervised_user::SupervisedUserInterstitial>>&
  interstitials_for_test() const {
    return supervised_user_interstitials_;
  }

  const std::set<std::string>& requested_hosts_for_test() const {
    return requested_hosts_;
  }

 private:
  friend class content::WebContentsUserData<SupervisedUserNavigationObserver>;

  explicit SupervisedUserNavigationObserver(content::WebContents* web_contents);

  void OnRequestBlockedInternal(const GURL& url,
                                supervised_user::FilteringBehaviorReason reason,
                                int64_t navigation_id,
                                content::FrameTreeNodeId frame_id,
                                const OnInterstitialResultCallback& callback);

  void URLFilterCheckCallback(const GURL& url,
                              int render_frame_process_id,
                              int render_frame_routing_id,
                              supervised_user::FilteringBehavior behavior,
                              supervised_user::FilteringBehaviorReason reason,
                              bool uncertain);

  void MaybeShowInterstitial(const GURL& url,
                             supervised_user::FilteringBehaviorReason reason,
                             bool initial_page_load,
                             int64_t navigation_id,
                             content::FrameTreeNodeId frame_id,
                             const OnInterstitialResultCallback& callback);

  // Filters the RenderFrameHost if render frame is live.
  void FilterRenderFrame(content::RenderFrameHost* render_frame_host);

  // supervised_user::mojom::SupervisedUserCommands implementation. Should not
  // be called when an interstitial is no longer showing. This should be
  // enforced by the mojo caller.
  void GoBack() override;
  void RequestUrlAccessRemote(RequestUrlAccessRemoteCallback callback) override;
  void RequestUrlAccessLocal(RequestUrlAccessLocalCallback callback) override;

  // When a remote URL approval request is successfully created, this method is
  // called asynchronously.
  void RequestCreated(RequestUrlAccessRemoteCallback callback,
                      const std::string& host,
                      bool successfully_created_request);

  // Called when the url filter changes i.e. allowlist or denylist change to
  // clear up  entries in |requested_hosts_| which have been allowed.
  void MaybeUpdateRequestedHosts();

  // Owned by SupervisedUserService.
  raw_ptr<supervised_user::SupervisedUserURLFilter> url_filter_;

  // Owned by SupervisedUserServiceFactory (lifetime of Profile).
  raw_ptr<supervised_user::SupervisedUserService> supervised_user_service_;

  // Keeps track of the blocked frames. It maps the frame's globally unique
  // id to its corresponding |SupervisedUserInterstitial| instance.
  std::map<content::FrameTreeNodeId,
           std::unique_ptr<supervised_user::SupervisedUserInterstitial>>
      supervised_user_interstitials_;

  std::set<std::string> requested_hosts_;

  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      blocked_navigations_;

  content::RenderFrameHostReceiverSet<
      supervised_user::mojom::SupervisedUserCommands>
      receivers_;

  base::WeakPtrFactory<SupervisedUserNavigationObserver> weak_ptr_factory_{
      this};

  void RecordPageLoadUKM(content::RenderFrameHost* render_frame_host);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

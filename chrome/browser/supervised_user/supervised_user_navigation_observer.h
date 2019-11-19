// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "chrome/common/supervised_user_commands.mojom.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SupervisedUserService;
class SupervisedUserInterstitial;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

class SupervisedUserNavigationObserver
    : public content::WebContentsUserData<SupervisedUserNavigationObserver>,
      public content::WebContentsObserver,
      public SupervisedUserServiceObserver,
      public supervised_user::mojom::SupervisedUserCommands {
 public:
  ~SupervisedUserNavigationObserver() override;

  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>&
  blocked_navigations() const {
    return blocked_navigations_;
  }

  // Called when a network request to |url| is blocked.
  static void OnRequestBlocked(
      content::WebContents* web_contents,
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason,
      int64_t navigation_id,
      int frame_id,
      const base::Callback<
          void(SupervisedUserNavigationThrottle::CallbackActions)>& callback);

  void UpdateMainFrameFilteringStatus(
      SupervisedUserURLFilter::FilteringBehavior behavior,
      supervised_user_error_page::FilteringBehaviorReason reason);

  SupervisedUserURLFilter::FilteringBehavior main_frame_filtering_behavior()
      const {
    return main_frame_filtering_behavior_;
  }

  supervised_user_error_page::FilteringBehaviorReason
  main_frame_filtering_behavior_reason() const {
    return main_frame_filtering_behavior_reason_;
  }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

  // Called when interstitial error page is no longer being shown in the main
  // frame.
  void OnInterstitialDone(int frame_id);

  const std::map<int, std::unique_ptr<SupervisedUserInterstitial>>&
  interstitials_for_test() {
    return supervised_user_interstitials_;
  }

 private:
  friend class content::WebContentsUserData<SupervisedUserNavigationObserver>;

  explicit SupervisedUserNavigationObserver(content::WebContents* web_contents);

  void OnRequestBlockedInternal(
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason,
      int64_t navigation_id,
      int frame_id,
      const base::Callback<
          void(SupervisedUserNavigationThrottle::CallbackActions)>& callback);

  void URLFilterCheckCallback(
      const GURL& url,
      int render_frame_process_id,
      int render_frame_routing_id,
      SupervisedUserURLFilter::FilteringBehavior behavior,
      supervised_user_error_page::FilteringBehaviorReason reason,
      bool uncertain);

  void MaybeShowInterstitial(
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason,
      bool initial_page_load,
      int64_t navigation_id,
      int frame_id,
      const base::Callback<
          void(SupervisedUserNavigationThrottle::CallbackActions)>& callback);

  // Filters the render frame host if render frame is live.
  void FilterRenderFrame(content::RenderFrameHost* render_frame_host);

  // supervised_user::mojom::SupervisedUserCommands implementation. Should not
  // be called when an interstitial is no longer showing. This should be
  // enforced by the mojo caller.
  void GoBack() override;
  void RequestPermission(RequestPermissionCallback callback) override;
  void Feedback() override;

  // Owned by SupervisedUserService.
  const SupervisedUserURLFilter* url_filter_;

  // Owned by SupervisedUserServiceFactory (lifetime of Profile).
  SupervisedUserService* supervised_user_service_;

  // Keeps track of the blocked frames. It maps the frame's globally unique
  // id to its corresponding |SupervisedUserInterstitial| instance.
  std::map<int, std::unique_ptr<SupervisedUserInterstitial>>
      supervised_user_interstitials_;

  SupervisedUserURLFilter::FilteringBehavior main_frame_filtering_behavior_ =
      SupervisedUserURLFilter::FilteringBehavior::ALLOW;
  supervised_user_error_page::FilteringBehaviorReason
      main_frame_filtering_behavior_reason_ =
          supervised_user_error_page::FilteringBehaviorReason::DEFAULT;

  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      blocked_navigations_;

  content::WebContentsFrameBindingSet<
      supervised_user::mojom::SupervisedUserCommands>
      binding_;

  base::WeakPtrFactory<SupervisedUserNavigationObserver> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserNavigationObserver);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_NAVIGATION_OBSERVER_H_

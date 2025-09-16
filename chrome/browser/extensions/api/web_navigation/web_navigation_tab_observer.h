// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_TAB_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_TAB_OBSERVER_H_

#include <memory>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

struct Event;

// Tab contents observer that forwards navigation events to the event router.
class WebNavigationTabObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebNavigationTabObserver> {
 public:
  WebNavigationTabObserver(const WebNavigationTabObserver&) = delete;
  WebNavigationTabObserver& operator=(const WebNavigationTabObserver&) = delete;

  ~WebNavigationTabObserver() override;

  // Returns the object for the given |web_contents|.
  static WebNavigationTabObserver* Get(content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // This method dispatches the already created onBeforeNavigate event.
  void DispatchCachedOnBeforeNavigate();

 private:
  explicit WebNavigationTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebNavigationTabObserver>;

  void HandleCommit(content::NavigationHandle* navigation_handle);
  void HandleError(content::NavigationHandle* navigation_handle);

  // True if the transition and target url correspond to a reference fragment
  // navigation.
  bool IsReferenceFragmentNavigation(content::RenderFrameHost* frame_host,
                                     const GURL& url);

  // Called when a RenderFrameHost goes into pending deletion. Stop tracking it
  // and its children.
  void RenderFrameHostPendingDeletion(content::RenderFrameHost*);

  // The latest onBeforeNavigate event this frame has generated. It is stored
  // as it might not be sent immediately, but delayed until the tab is added to
  // the tab strip and is ready to dispatch events.
  std::unique_ptr<Event> pending_on_before_navigate_event_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_TAB_OBSERVER_H_

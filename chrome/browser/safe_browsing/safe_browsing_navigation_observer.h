// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_H_

#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

namespace safe_browsing {
class SafeBrowsingNavigationObserverManager;

// Struct to record the details of a navigation event for any frame.
// This information will be used to fill referrer chain info in various Safe
// Browsing requests and reports.
struct NavigationEvent {
  NavigationEvent();
  NavigationEvent(NavigationEvent&& nav_event);
  NavigationEvent& operator=(NavigationEvent&& nav_event);
  ~NavigationEvent();

  // URL that caused this navigation to occur.
  GURL source_url;

  // Main frame url of the source_url. Could be the same as source_url, if
  // source_url was loaded in main frame.
  GURL source_main_frame_url;

  // The original request URL of this navigation.
  GURL original_request_url;

  // Server redirect url chain. Empty if there is no server redirect. If set,
  // last url in this vector is the destination url.
  std::vector<GURL> server_redirect_urls;

  // Which tab contains the frame with source_url. Tab ID is returned by
  // SessionTabHelper::IdForTab. This ID is immutable for a given tab and unique
  // across Chrome within the current session.
  SessionID source_tab_id;

  // Which tab this request url is targeting to.
  SessionID target_tab_id;

  // Frame tree node ID of the frame where this navigation takes place.
  int frame_id;

  // When this NavigationEvent was last updated.
  base::Time last_updated;

  // If this navigation is triggered by browser or renderer, and if it is
  // associated with any user gesture.
  ReferrerChainEntry::NavigationInitiation navigation_initiation;

  // Whether this a committed navigation. Navigation leads to download is not
  // committed.
  bool has_committed;

  // Whether we think this event was launched by an external application.
  bool maybe_launched_by_external_application;

  const GURL& GetDestinationUrl() const {
    if (!server_redirect_urls.empty())
      return server_redirect_urls.back();
    else
      return original_request_url;
  }

  bool IsUserInitiated() const {
    return navigation_initiation == ReferrerChainEntry::BROWSER_INITIATED ||
           navigation_initiation ==
               ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  }
};

// Structure to keep track of resolved IP address of a host.
struct ResolvedIPAddress {
  ResolvedIPAddress() : timestamp(base::Time::Now()), ip() {}
  ResolvedIPAddress(base::Time timestamp, const std::string& ip)
      : timestamp(timestamp), ip(ip) {}
  base::Time timestamp;  // Timestamp of when we get the resolved IP.
  std::string ip;        // Resolved IP address
};

// Observes the navigation events for a single WebContents (both main-frame
// and sub-frame navigations).
class SafeBrowsingNavigationObserver : public base::SupportsUserData::Data,
                                       public content::WebContentsObserver,
                                       public content_settings::Observer {
 public:
  static void MaybeCreateForWebContents(
      content::WebContents* web_contents);

  static SafeBrowsingNavigationObserver* FromWebContents(
      content::WebContents* web_contents);

  SafeBrowsingNavigationObserver(
      content::WebContents* contents,
      const scoped_refptr<SafeBrowsingNavigationObserverManager>& manager);

  ~SafeBrowsingNavigationObserver() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SBNavigationObserverTest, TestContentSettingChange);
  typedef std::unordered_map<content::NavigationHandle*,
                             std::unique_ptr<NavigationEvent>>
      NavigationHandleMap;

  void OnUserInteraction();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;
  void WebContentsDestroyed() override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // content_settings::Observer overrides.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // Map keyed on NavigationHandle* to keep track of all the ongoing navigation
  // events. NavigationHandle pointers are owned by RenderFrameHost. Since a
  // NavigationHandle object will be destructed after navigation is done,
  // at the end of DidFinishNavigation(...) corresponding entries in this map
  // will be removed from navigation_handle_map_ and added to
  // SafeBrowsingNavigationObserverManager::navigation_map_.
  NavigationHandleMap navigation_handle_map_;

  scoped_refptr<SafeBrowsingNavigationObserverManager> manager_;

  // If the observed WebContents just got an user gesture.
  bool has_user_gesture_;

  base::Time last_user_gesture_timestamp_;
  ScopedObserver<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingNavigationObserver);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_H_

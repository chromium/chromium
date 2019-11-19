// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "net/base/ip_endpoint.h"

using content::WebContents;

namespace {
const char kWebContentsUserDataKey[] =
    "web_contents_safe_browsing_navigation_observer";
}  // namespace

namespace safe_browsing {

// SafeBrowsingNavigationObserver::NavigationEvent-----------------------------
NavigationEvent::NavigationEvent()
    : source_url(),
      source_main_frame_url(),
      original_request_url(),
      source_tab_id(SessionID::InvalidValue()),
      target_tab_id(SessionID::InvalidValue()),
      frame_id(-1),
      last_updated(base::Time::Now()),
      navigation_initiation(ReferrerChainEntry::UNDEFINED),
      has_committed(false),
      maybe_launched_by_external_application() {}

NavigationEvent::NavigationEvent(NavigationEvent&& nav_event)
    : source_url(std::move(nav_event.source_url)),
      source_main_frame_url(std::move(nav_event.source_main_frame_url)),
      original_request_url(std::move(nav_event.original_request_url)),
      server_redirect_urls(std::move(nav_event.server_redirect_urls)),
      source_tab_id(std::move(nav_event.source_tab_id)),
      target_tab_id(std::move(nav_event.target_tab_id)),
      frame_id(nav_event.frame_id),
      last_updated(nav_event.last_updated),
      navigation_initiation(nav_event.navigation_initiation),
      has_committed(nav_event.has_committed),
      maybe_launched_by_external_application(
          nav_event.maybe_launched_by_external_application) {}

NavigationEvent& NavigationEvent::operator=(NavigationEvent&& nav_event) {
  source_url = std::move(nav_event.source_url);
  source_main_frame_url = std::move(nav_event.source_main_frame_url);
  original_request_url = std::move(nav_event.original_request_url);
  source_tab_id = nav_event.source_tab_id;
  target_tab_id = nav_event.target_tab_id;
  frame_id = nav_event.frame_id;
  last_updated = nav_event.last_updated;
  navigation_initiation = nav_event.navigation_initiation;
  has_committed = nav_event.has_committed;
  maybe_launched_by_external_application =
      nav_event.maybe_launched_by_external_application;
  server_redirect_urls = std::move(nav_event.server_redirect_urls);
  return *this;
}

NavigationEvent::~NavigationEvent() {}

// SafeBrowsingNavigationObserver --------------------------------------------

// static
void SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (FromWebContents(web_contents))
    return;

  if (safe_browsing::SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    web_contents->SetUserData(
        kWebContentsUserDataKey,
        std::make_unique<SafeBrowsingNavigationObserver>(
            web_contents, g_browser_process->safe_browsing_service()
                              ->navigation_observer_manager()));
  }
}

// static
SafeBrowsingNavigationObserver* SafeBrowsingNavigationObserver::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<SafeBrowsingNavigationObserver*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
}

SafeBrowsingNavigationObserver::SafeBrowsingNavigationObserver(
    content::WebContents* contents,
    const scoped_refptr<SafeBrowsingNavigationObserverManager>& manager)
    : content::WebContentsObserver(contents),
      manager_(manager),
      has_user_gesture_(false),
      last_user_gesture_timestamp_(base::Time()) {
  content_settings_observer_.Add(HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())));
}

SafeBrowsingNavigationObserver::~SafeBrowsingNavigationObserver() {}

void SafeBrowsingNavigationObserver::OnUserInteraction() {
  last_user_gesture_timestamp_ = base::Time::Now();
  has_user_gesture_ = true;
  manager_->RecordUserGestureForWebContents(web_contents(),
                                            last_user_gesture_timestamp_);
}

// Called when a navigation starts in the WebContents. |navigation_handle|
// parameter is unique to this navigation, which will appear in the following
// DidRedirectNavigation, and DidFinishNavigation too.
void SafeBrowsingNavigationObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Treat a browser-initiated navigation as a user interaction.
  if (!navigation_handle->IsRendererInitiated())
    OnUserInteraction();

  // Ignores navigation caused by back/forward.
  if (navigation_handle->GetPageTransition() &
      ui::PAGE_TRANSITION_FORWARD_BACK) {
    return;
  }

  // Ignores reloads
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }

  std::unique_ptr<NavigationEvent> nav_event =
      std::make_unique<NavigationEvent>();
  auto it = navigation_handle_map_.find(navigation_handle);
  // It is possible to see multiple DidStartNavigation(..) with the same
  // navigation_handle (e.g. cross-process transfer). If that's the case,
  // we need to copy the navigation_initiation field.
  if (it != navigation_handle_map_.end() &&
      it->second->navigation_initiation != ReferrerChainEntry::UNDEFINED) {
    nav_event->navigation_initiation = it->second->navigation_initiation;
  } else {
    // If this is the first time we see this navigation_handle, create a new
    // NavigationEvent, and decide if it is triggered by user.
    if (!navigation_handle->IsRendererInitiated()) {
      nav_event->navigation_initiation = ReferrerChainEntry::BROWSER_INITIATED;
    } else if (has_user_gesture_ &&
               !SafeBrowsingNavigationObserverManager::IsUserGestureExpired(
                   last_user_gesture_timestamp_)) {
      nav_event->navigation_initiation =
          ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
    } else {
      nav_event->navigation_initiation =
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE;
    }
    if (has_user_gesture_) {
      manager_->OnUserGestureConsumed(web_contents(),
                                      last_user_gesture_timestamp_);
      has_user_gesture_ = false;
    }
  }

  // All the other fields are reconstructed based on current content of
  // navigation_handle.
  nav_event->frame_id = navigation_handle->GetFrameTreeNodeId();

  // If there was a URL previously committed in the current RenderFrameHost,
  // set it as the source url of this navigation. Otherwise, this is the
  // first url going to commit in this frame. We set navigation_handle's URL as
  // the source url.
  int current_process_id =
      navigation_handle->GetStartingSiteInstance()->GetProcess()->GetID();
  content::RenderFrameHost* current_frame_host =
      navigation_handle->GetWebContents()->FindFrameByFrameTreeNodeId(
          nav_event->frame_id, current_process_id);
  // For browser initiated navigation (e.g. from address bar or bookmark), we
  // don't fill the source_url to prevent attributing navigation to the last
  // committed navigation.
  if (navigation_handle->IsRendererInitiated() && current_frame_host &&
      current_frame_host->GetLastCommittedURL().is_valid()) {
    nav_event->source_url = SafeBrowsingNavigationObserverManager::ClearURLRef(
        current_frame_host->GetLastCommittedURL());
  }
  nav_event->original_request_url =
      SafeBrowsingNavigationObserverManager::ClearURLRef(
          navigation_handle->GetURL());

  nav_event->source_tab_id =
      SessionTabHelper::IdForTab(navigation_handle->GetWebContents());

  if (navigation_handle->IsInMainFrame()) {
    nav_event->source_main_frame_url = nav_event->source_url;
  } else {
    nav_event->source_main_frame_url =
        SafeBrowsingNavigationObserverManager::ClearURLRef(
            navigation_handle->GetWebContents()->GetLastCommittedURL());
  }
  navigation_handle_map_[navigation_handle] = std::move(nav_event);
}

void SafeBrowsingNavigationObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  // We should have already seen this navigation_handle in DidStartNavigation.
  if (navigation_handle_map_.find(navigation_handle) ==
      navigation_handle_map_.end()) {
    return;
  }
  NavigationEvent* nav_event = navigation_handle_map_[navigation_handle].get();
  nav_event->server_redirect_urls.push_back(
      SafeBrowsingNavigationObserverManager::ClearURLRef(
          navigation_handle->GetURL()));
  nav_event->last_updated = base::Time::Now();
}

void SafeBrowsingNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if ((navigation_handle->HasCommitted() || navigation_handle->IsDownload()) &&
      !navigation_handle->GetSocketAddress().address().empty()) {
    manager_->RecordHostToIpMapping(
        navigation_handle->GetURL().host(),
        navigation_handle->GetSocketAddress().ToStringWithoutPort());
  }

  if (navigation_handle_map_.find(navigation_handle) ==
      navigation_handle_map_.end()) {
    return;
  }

  // If it is an error page, we ignore this navigation.
  if (navigation_handle->IsErrorPage()) {
    navigation_handle_map_.erase(navigation_handle);
    return;
  }
  NavigationEvent* nav_event = navigation_handle_map_[navigation_handle].get();

  nav_event->maybe_launched_by_external_application =
      PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_event->has_committed = navigation_handle->HasCommitted();
  nav_event->target_tab_id =
      SessionTabHelper::IdForTab(navigation_handle->GetWebContents());
  nav_event->last_updated = base::Time::Now();

  manager_->RecordNavigationEvent(
      std::move(navigation_handle_map_[navigation_handle]));
  navigation_handle_map_.erase(navigation_handle);
}

void SafeBrowsingNavigationObserver::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  OnUserInteraction();
}

void SafeBrowsingNavigationObserver::WebContentsDestroyed() {
  manager_->OnWebContentDestroyed(web_contents());
  web_contents()->RemoveUserData(kWebContentsUserDataKey);
  // web_contents is null after this function.
}

void SafeBrowsingNavigationObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  manager_->RecordNewWebContents(
      web_contents(), source_render_frame_host->GetProcess()->GetID(),
      source_render_frame_host->GetRoutingID(), url, transition, new_contents,
      renderer_initiated);
}

void SafeBrowsingNavigationObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // For all the content settings that can be changed via page info UI, we
  // assume there is a user gesture associated with the content setting change.
  if (web_contents() &&
      primary_pattern.Matches(web_contents()->GetLastCommittedURL()) &&
      PageInfoUI::ContentSettingsTypeInPageInfo(content_type)) {
    OnUserInteraction();
  }
}

}  // namespace safe_browsing

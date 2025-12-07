// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"

#include <string>

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/api/web_navigation.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace extensions {

namespace web_navigation = api::web_navigation;

WebNavigationTabObserver::WebNavigationTabObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<WebNavigationTabObserver>(*web_contents) {}

WebNavigationTabObserver::~WebNavigationTabObserver() = default;

// static
WebNavigationTabObserver* WebNavigationTabObserver::Get(
    content::WebContents* web_contents) {
  return FromWebContents(web_contents);
}

void WebNavigationTabObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto* navigation_state =
      FrameNavigationState::GetForCurrentDocument(render_frame_host);
  if (navigation_state && navigation_state->CanSendEvents() &&
      !navigation_state->GetDocumentLoadCompleted()) {
    web_navigation_api_helpers::DispatchOnErrorOccurred(
        web_contents(), render_frame_host, navigation_state->GetUrl(),
        net::ERR_ABORTED);
    navigation_state->SetErrorOccurredInFrame();
  }
}

void WebNavigationTabObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (old_host) {
    RenderFrameHostPendingDeletion(old_host);
  }
}

void WebNavigationTabObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument() ||
      !FrameNavigationState::IsValidUrl(navigation_handle->GetURL())) {
    return;
  }

  pending_on_before_navigate_event_ =
      web_navigation_api_helpers::CreateOnBeforeNavigateEvent(
          navigation_handle);

  // Only dispatch the onBeforeNavigate event if the associated WebContents
  // is already added to the tab strip. Otherwise the event should be delayed
  // and sent after the addition, to preserve the ordering of events.
  //
  // TODO(nasko|devlin): This check is necessary because chrome::Navigate()
  // begins the navigation before adding the tab to the TabStripModel, and it
  // is used an indication of that. It would be best if instead it was known
  // when the tab was created and immediately sent the created event instead of
  // waiting for the later TabStripModel kInserted change, but this appears to
  // work for now.
  if (ExtensionTabUtil::GetTabById(ExtensionTabUtil::GetTabId(web_contents()),
                                   web_contents()->GetBrowserContext(), false,
                                   nullptr)) {
    DispatchCachedOnBeforeNavigate();
  }
}

void WebNavigationTabObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If there has been a DidStartNavigation call before the tab was ready to
  // dispatch events, ensure that it is sent before processing the
  // DidFinishNavigation.
  // Note: This is exercised by WebNavigationApiTest.TargetBlankIncognito.
  DispatchCachedOnBeforeNavigate();

  if (navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage()) {
    HandleCommit(navigation_handle);
    return;
  }

  HandleError(navigation_handle);
}

void WebNavigationTabObserver::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  auto* navigation_state =
      FrameNavigationState::GetForCurrentDocument(render_frame_host);
  if (!navigation_state || !navigation_state->CanSendEvents()) {
    return;
  }

  navigation_state->SetParsingFinished();
  web_navigation_api_helpers::DispatchOnDOMContentLoaded(
      web_contents(), render_frame_host, navigation_state->GetUrl());

  if (!navigation_state->GetDocumentLoadCompleted()) {
    return;
  }

  // The load might already have finished by the time we finished parsing. For
  // compatibility reasons, we artificially delay the load completed signal
  // until after parsing was completed.
  web_navigation_api_helpers::DispatchOnCompleted(
      web_contents(), render_frame_host, navigation_state->GetUrl());
}

void WebNavigationTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  auto* navigation_state =
      FrameNavigationState::GetForCurrentDocument(render_frame_host);
  // When showing replacement content, we might get load signals for frames
  // that weren't regularly loaded.
  if (!navigation_state) {
    return;
  }

  navigation_state->SetDocumentLoadCompleted();
  if (!navigation_state->CanSendEvents()) {
    return;
  }

  // A new navigation might have started before the old one completed.
  // Ignore the old navigation completion in that case.
  if (navigation_state->GetUrl() != validated_url) {
    return;
  }

  // The load might already have finished by the time we finished parsing. For
  // compatibility reasons, we artificially delay the load completed signal
  // until after parsing was completed.
  if (!navigation_state->GetParsingFinished()) {
    return;
  }
  web_navigation_api_helpers::DispatchOnCompleted(
      web_contents(), render_frame_host, navigation_state->GetUrl());
}

void WebNavigationTabObserver::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  auto* navigation_state =
      FrameNavigationState::GetForCurrentDocument(render_frame_host);
  // When showing replacement content, we might get load signals for frames
  // that weren't regularly loaded.
  if (!navigation_state) {
    return;
  }

  if (navigation_state->CanSendEvents()) {
    web_navigation_api_helpers::DispatchOnErrorOccurred(
        web_contents(), render_frame_host, navigation_state->GetUrl(),
        error_code);
  }
  navigation_state->SetErrorOccurredInFrame();
}

void WebNavigationTabObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  auto* navigation_state =
      FrameNavigationState::GetForCurrentDocument(source_render_frame_host);
  if (!navigation_state || !navigation_state->CanSendEvents()) {
    return;
  }

  // We only send the onCreatedNavigationTarget if we end up creating a new
  // window.
  if (disposition != WindowOpenDisposition::SINGLETON_TAB &&
      disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_POPUP &&
      disposition != WindowOpenDisposition::NEW_WINDOW &&
      disposition != WindowOpenDisposition::OFF_THE_RECORD) {
    return;
  }

  WebNavigationAPI* api = WebNavigationAPI::GetFactoryInstance()->Get(
      web_contents()->GetBrowserContext());
  if (!api) {
    return;  // Possible in unit tests.
  }
  WebNavigationEventRouter* router = api->web_navigation_event_router_.get();
  if (!router) {
    return;
  }

  bool new_contents_is_present_in_tabstrip = false;
  TabListInterface* ignored_tab_list_interface = nullptr;
  int ignored_tab_index = -1;
  // It's possible `new_contents` could be null.
  if (new_contents) {
    new_contents_is_present_in_tabstrip = ExtensionTabUtil::GetTabListInterface(
        *new_contents, &ignored_tab_list_interface, &ignored_tab_index);
  }
  router->RecordNewWebContents(
      web_contents(), source_render_frame_host->GetProcess()->GetDeprecatedID(),
      source_render_frame_host->GetRoutingID(), url, new_contents,
      !new_contents_is_present_in_tabstrip);
}

void WebNavigationTabObserver::DispatchCachedOnBeforeNavigate() {
  if (!pending_on_before_navigate_event_) {
    return;
  }

  // EventRouter can be null in unit tests.
  EventRouter* event_router =
      EventRouter::Get(web_contents()->GetBrowserContext());
  if (event_router) {
    event_router->BroadcastEvent(std::move(pending_on_before_navigate_event_));
  }
}

void WebNavigationTabObserver::HandleCommit(
    content::NavigationHandle* navigation_handle) {
  bool is_reference_fragment_navigation =
      navigation_handle->IsSameDocument() &&
      IsReferenceFragmentNavigation(navigation_handle->GetRenderFrameHost(),
                                    navigation_handle->GetURL());

  FrameNavigationState::GetOrCreateForCurrentDocument(
      navigation_handle->GetRenderFrameHost())
      ->StartTrackingDocumentLoad(
          navigation_handle->GetURL(), navigation_handle->IsSameDocument(),
          navigation_handle->IsServedFromBackForwardCache(),
          /*is_error_page=*/false);

  events::HistogramValue histogram_value = events::UNKNOWN;
  std::string event_name;
  if (is_reference_fragment_navigation) {
    histogram_value = events::WEB_NAVIGATION_ON_REFERENCE_FRAGMENT_UPDATED;
    event_name = web_navigation::OnReferenceFragmentUpdated::kEventName;
  } else if (navigation_handle->IsSameDocument()) {
    histogram_value = events::WEB_NAVIGATION_ON_HISTORY_STATE_UPDATED;
    event_name = web_navigation::OnHistoryStateUpdated::kEventName;
  } else {
    histogram_value = events::WEB_NAVIGATION_ON_COMMITTED;
    event_name = web_navigation::OnCommitted::kEventName;
  }
  web_navigation_api_helpers::DispatchOnCommitted(histogram_value, event_name,
                                                  navigation_handle);

  if (navigation_handle->IsServedFromBackForwardCache()) {
    web_navigation_api_helpers::DispatchOnCompleted(
        navigation_handle->GetWebContents(),
        navigation_handle->GetRenderFrameHost(), navigation_handle->GetURL());
  }
}

void WebNavigationTabObserver::HandleError(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted()) {
    FrameNavigationState::GetOrCreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost())
        ->StartTrackingDocumentLoad(navigation_handle->GetURL(),
                                    navigation_handle->IsSameDocument(),
                                    /*is_from_back_forward_cache=*/false,
                                    /*is_error_page=*/true);
  }

  web_navigation_api_helpers::DispatchOnErrorOccurred(navigation_handle);
}

bool WebNavigationTabObserver::IsReferenceFragmentNavigation(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  auto* navigation_state =
      FrameNavigationState::GetForCurrentDocument(render_frame_host);

  GURL existing_url = navigation_state ? navigation_state->GetUrl() : GURL();
  if (existing_url == url) {
    return false;
  }

  return existing_url.EqualsIgnoringRef(url);
}

void WebNavigationTabObserver::RenderFrameHostPendingDeletion(
    content::RenderFrameHost* pending_delete_render_frame_host) {
  // The |pending_delete_render_frame_host| and its children are now pending
  // deletion. Stop tracking them.

  pending_delete_render_frame_host->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        auto* navigation_state =
            FrameNavigationState::GetForCurrentDocument(render_frame_host);
        if (navigation_state) {
          RenderFrameDeleted(render_frame_host);
          FrameNavigationState::DeleteForCurrentDocument(render_frame_host);
        }
      });
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebNavigationTabObserver);

}  // namespace extensions

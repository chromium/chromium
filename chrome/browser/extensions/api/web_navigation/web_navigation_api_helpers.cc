// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/web_navigation.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/common/event_filtering_info.h"
#include "net/base/net_errors.h"
#include "ui/base/page_transition_types.h"

namespace extensions {

namespace web_navigation = api::web_navigation;

namespace web_navigation_api_helpers {

namespace {

// Returns |time| as milliseconds since the epoch.
double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

// Dispatches events to the extension message service.
void DispatchEvent(content::BrowserContext* browser_context,
                   std::unique_ptr<Event> event,
                   const GURL& url) {
  EventFilteringInfo info;
  info.url = url;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  EventRouter* event_router = EventRouter::Get(profile);
  if (profile && event_router) {
    DCHECK_EQ(profile, event->restrict_to_browser_context);
    event->filter_info = info;
    event_router->BroadcastEvent(std::move(event));
  }
}

}  // namespace

// Constructs an onBeforeNavigate event.
std::unique_ptr<Event> CreateOnBeforeNavigateEvent(
    content::NavigationHandle* navigation_handle) {
  GURL url(navigation_handle->GetURL());

  web_navigation::OnBeforeNavigate::Details details;
  details.tab_id =
      ExtensionTabUtil::GetTabId(navigation_handle->GetWebContents());
  details.url = url.spec();
  details.process_id = -1;
  details.frame_id = ExtensionApiFrameIdMap::GetFrameId(navigation_handle);
  details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(navigation_handle);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_BEFORE_NAVIGATE,
      web_navigation::OnBeforeNavigate::kEventName,
      web_navigation::OnBeforeNavigate::Create(details),
      navigation_handle->GetWebContents()->GetBrowserContext());

  EventFilteringInfo info;
  info.url = navigation_handle->GetURL();
  event->filter_info = info;

  return event;
}

// Constructs and dispatches an onCommitted, onReferenceFragmentUpdated
// or onHistoryStateUpdated event.
void DispatchOnCommitted(events::HistogramValue histogram_value,
                         const std::string& event_name,
                         content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  GURL url(navigation_handle->GetURL());
  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();
  ui::PageTransition transition_type = navigation_handle->GetPageTransition();

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger(web_navigation_api_constants::kTabIdKey,
                   ExtensionTabUtil::GetTabId(web_contents));
  dict->SetString(web_navigation_api_constants::kUrlKey, url.spec());
  dict->SetInteger(web_navigation_api_constants::kProcessIdKey,
                   frame_host->GetProcess()->GetID());
  dict->SetInteger(web_navigation_api_constants::kFrameIdKey,
                   ExtensionApiFrameIdMap::GetFrameId(frame_host));
  dict->SetInteger(web_navigation_api_constants::kParentFrameIdKey,
                   ExtensionApiFrameIdMap::GetParentFrameId(frame_host));

  if (navigation_handle->WasServerRedirect()) {
    transition_type = ui::PageTransitionFromInt(
        transition_type | ui::PAGE_TRANSITION_SERVER_REDIRECT);
  }

  std::string transition_type_string =
      ui::PageTransitionGetCoreTransitionString(transition_type);
  // For webNavigation API backward compatibility, keep "start_page" even after
  // renamed to "auto_toplevel".
  if (ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_AUTO_TOPLEVEL))
    transition_type_string = "start_page";
  dict->SetString(web_navigation_api_constants::kTransitionTypeKey,
                  transition_type_string);
  auto qualifiers = std::make_unique<base::ListValue>();
  if (transition_type & ui::PAGE_TRANSITION_CLIENT_REDIRECT)
    qualifiers->AppendString("client_redirect");
  if (transition_type & ui::PAGE_TRANSITION_SERVER_REDIRECT)
    qualifiers->AppendString("server_redirect");
  if (transition_type & ui::PAGE_TRANSITION_FORWARD_BACK)
    qualifiers->AppendString("forward_back");
  if (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)
    qualifiers->AppendString("from_address_bar");
  dict->Set(web_navigation_api_constants::kTransitionQualifiersKey,
            std::move(qualifiers));
  dict->SetDouble(web_navigation_api_constants::kTimeStampKey,
                  MilliSecondsFromTime(base::Time::Now()));
  args->Append(std::move(dict));

  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();
  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(args), browser_context);
  DispatchEvent(browser_context, std::move(event), url);
}

// Constructs and dispatches an onDOMContentLoaded event.
void DispatchOnDOMContentLoaded(content::WebContents* web_contents,
                                content::RenderFrameHost* frame_host,
                                const GURL& url) {
  web_navigation::OnDOMContentLoaded::Details details;
  details.tab_id = ExtensionTabUtil::GetTabId(web_contents);
  details.url = url.spec();
  details.process_id = frame_host->GetProcess()->GetID();
  details.frame_id = ExtensionApiFrameIdMap::GetFrameId(frame_host);
  details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(frame_host);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_DOM_CONTENT_LOADED,
      web_navigation::OnDOMContentLoaded::kEventName,
      web_navigation::OnDOMContentLoaded::Create(details), browser_context);
  DispatchEvent(browser_context, std::move(event), url);
}

// Constructs and dispatches an onCompleted event.
void DispatchOnCompleted(content::WebContents* web_contents,
                         content::RenderFrameHost* frame_host,
                         const GURL& url) {
  web_navigation::OnCompleted::Details details;
  details.tab_id = ExtensionTabUtil::GetTabId(web_contents);
  details.url = url.spec();
  details.process_id = frame_host->GetProcess()->GetID();
  details.frame_id = ExtensionApiFrameIdMap::GetFrameId(frame_host);
  details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(frame_host);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_COMPLETED,
      web_navigation::OnCompleted::kEventName,
      web_navigation::OnCompleted::Create(details), browser_context);
  DispatchEvent(browser_context, std::move(event), url);
}

// Constructs and dispatches an onCreatedNavigationTarget event.
void DispatchOnCreatedNavigationTarget(
    int source_tab_id,
    int source_render_process_id,
    int source_extension_frame_id,
    content::BrowserContext* browser_context,
    content::WebContents* target_web_contents,
    const GURL& target_url) {
  // Check that the tab is already inserted into a tab strip model. This code
  // path is exercised by ExtensionApiTest.WebNavigationRequestOpenTab.
  DCHECK(ExtensionTabUtil::GetTabById(
      ExtensionTabUtil::GetTabId(target_web_contents),
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext()),
      false, nullptr));

  web_navigation::OnCreatedNavigationTarget::Details details;
  details.source_tab_id = source_tab_id;
  details.source_process_id = source_render_process_id;
  details.source_frame_id = source_extension_frame_id;
  details.url = target_url.possibly_invalid_spec();
  details.tab_id = ExtensionTabUtil::GetTabId(target_web_contents);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_CREATED_NAVIGATION_TARGET,
      web_navigation::OnCreatedNavigationTarget::kEventName,
      web_navigation::OnCreatedNavigationTarget::Create(details),
      browser_context);
  DispatchEvent(browser_context, std::move(event), target_url);

  // If the target WebContents already received the onBeforeNavigate event,
  // send it immediately after the onCreatedNavigationTarget above.
  WebNavigationTabObserver* target_observer =
      WebNavigationTabObserver::Get(target_web_contents);
  target_observer->DispatchCachedOnBeforeNavigate();
}

// Constructs and dispatches an onErrorOccurred event.
void DispatchOnErrorOccurred(content::WebContents* web_contents,
                             content::RenderFrameHost* frame_host,
                             const GURL& url,
                             int error_code) {
  web_navigation::OnErrorOccurred::Details details;
  details.tab_id = ExtensionTabUtil::GetTabId(web_contents);
  details.url = url.spec();
  details.process_id = frame_host->GetProcess()->GetID();
  details.frame_id = ExtensionApiFrameIdMap::GetFrameId(frame_host);
  details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(frame_host);
  details.error = net::ErrorToString(error_code);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  auto event =
      std::make_unique<Event>(events::WEB_NAVIGATION_ON_ERROR_OCCURRED,
                              web_navigation::OnErrorOccurred::kEventName,
                              web_navigation::OnErrorOccurred::Create(details),
                              web_contents->GetBrowserContext());
  DispatchEvent(browser_context, std::move(event), url);
}

void DispatchOnErrorOccurred(content::NavigationHandle* navigation_handle) {
  web_navigation::OnErrorOccurred::Details details;
  details.tab_id =
      ExtensionTabUtil::GetTabId(navigation_handle->GetWebContents());
  details.url = navigation_handle->GetURL().spec();
  details.process_id = -1;
  details.frame_id = ExtensionApiFrameIdMap::GetFrameId(navigation_handle);
  details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(navigation_handle);
  details.error = (navigation_handle->GetNetErrorCode() != net::OK)
                      ? net::ErrorToString(navigation_handle->GetNetErrorCode())
                      : net::ErrorToString(net::ERR_ABORTED);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();
  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_ERROR_OCCURRED,
      web_navigation::OnErrorOccurred::kEventName,
      web_navigation::OnErrorOccurred::Create(details), browser_context);
  DispatchEvent(browser_context, std::move(event), navigation_handle->GetURL());
}

// Constructs and dispatches an onTabReplaced event.
void DispatchOnTabReplaced(
    content::WebContents* old_web_contents,
    content::BrowserContext* browser_context,
    content::WebContents* new_web_contents) {
  web_navigation::OnTabReplaced::Details details;
  details.replaced_tab_id = ExtensionTabUtil::GetTabId(old_web_contents);
  details.tab_id = ExtensionTabUtil::GetTabId(new_web_contents);
  details.time_stamp = MilliSecondsFromTime(base::Time::Now());

  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_TAB_REPLACED,
      web_navigation::OnTabReplaced::kEventName,
      web_navigation::OnTabReplaced::Create(details), browser_context);
  DispatchEvent(browser_context, std::move(event), GURL());
}

}  // namespace web_navigation_api_helpers

}  // namespace extensions

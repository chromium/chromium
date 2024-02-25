// Copyright 2012 The Chromium Authors
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
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "net/base/net_errors.h"
#include "ui/base/page_transition_types.h"

namespace extensions {

namespace web_navigation = api::web_navigation;

namespace web_navigation_api_helpers {

namespace {

// Dispatches events to the extension message service.
void DispatchEvent(content::BrowserContext* browser_context,
                   std::unique_ptr<Event> event,
                   const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  EventRouter* event_router = EventRouter::Get(profile);
  if (profile && event_router) {
    mojom::EventFilteringInfoPtr info = mojom::EventFilteringInfo::New();
    info->url = url;
    DCHECK_EQ(profile, event->restrict_to_browser_context);
    event->filter_info = std::move(info);
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
  // There is no documentId for this event because the document has not
  // been created yet. It will first appear in the OnCommitted event. The
  // frameId can be used to associate OnBeforeNavigate and OnCommitted together.
  details.frame_id = ExtensionApiFrameIdMap::GetFrameId(navigation_handle);
  details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(navigation_handle);
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          navigation_handle->GetParentFrameOrOuterDocument()) {
    details.parent_document_id =
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString();
  }
  details.frame_type = ExtensionApiFrameIdMap::GetFrameType(navigation_handle);
  details.document_lifecycle =
      ExtensionApiFrameIdMap::GetDocumentLifecycle(navigation_handle);
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_BEFORE_NAVIGATE,
      web_navigation::OnBeforeNavigate::kEventName,
      web_navigation::OnBeforeNavigate::Create(details),
      navigation_handle->GetWebContents()->GetBrowserContext());

  mojom::EventFilteringInfoPtr info = mojom::EventFilteringInfo::New();
  info->url = navigation_handle->GetURL();
  event->filter_info = std::move(info);

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

  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(web_navigation_api_constants::kTabIdKey,
           ExtensionTabUtil::GetTabId(web_contents));
  dict.Set(web_navigation_api_constants::kUrlKey, url.spec());
  dict.Set(web_navigation_api_constants::kProcessIdKey,
           frame_host->GetProcess()->GetID());
  dict.Set(web_navigation_api_constants::kFrameIdKey,
           ExtensionApiFrameIdMap::GetFrameId(frame_host));
  dict.Set(web_navigation_api_constants::kParentFrameIdKey,
           ExtensionApiFrameIdMap::GetParentFrameId(frame_host));
  dict.Set(web_navigation_api_constants::kDocumentIdKey,
           ExtensionApiFrameIdMap::GetDocumentId(frame_host).ToString());
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          frame_host->GetParentOrOuterDocument()) {
    dict.Set(
        web_navigation_api_constants::kParentDocumentIdKey,
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString());
  }
  dict.Set(web_navigation_api_constants::kFrameTypeKey,
           ToString(ExtensionApiFrameIdMap::GetFrameType(frame_host)));
  dict.Set(web_navigation_api_constants::kDocumentLifecycleKey,
           ToString(ExtensionApiFrameIdMap::GetDocumentLifecycle(frame_host)));

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
  dict.Set(web_navigation_api_constants::kTransitionTypeKey,
           transition_type_string);
  base::Value::List qualifiers;
  if (transition_type & ui::PAGE_TRANSITION_CLIENT_REDIRECT)
    qualifiers.Append("client_redirect");
  if (transition_type & ui::PAGE_TRANSITION_SERVER_REDIRECT)
    qualifiers.Append("server_redirect");
  if (transition_type & ui::PAGE_TRANSITION_FORWARD_BACK)
    qualifiers.Append("forward_back");
  if (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)
    qualifiers.Append("from_address_bar");
  dict.Set(web_navigation_api_constants::kTransitionQualifiersKey,
           std::move(qualifiers));
  dict.Set(web_navigation_api_constants::kTimeStampKey,
           base::Time::Now().InMillisecondsFSinceUnixEpoch());
  args.Append(std::move(dict));

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
  details.document_id =
      ExtensionApiFrameIdMap::GetDocumentId(frame_host).ToString();
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          frame_host->GetParentOrOuterDocument()) {
    details.parent_document_id =
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString();
  }
  details.frame_type = ExtensionApiFrameIdMap::GetFrameType(frame_host);
  details.document_lifecycle =
      ExtensionApiFrameIdMap::GetDocumentLifecycle(frame_host);
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

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
  details.document_id =
      ExtensionApiFrameIdMap::GetDocumentId(frame_host).ToString();
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          frame_host->GetParentOrOuterDocument()) {
    details.parent_document_id =
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString();
  }
  details.frame_type = ExtensionApiFrameIdMap::GetFrameType(frame_host);
  details.document_lifecycle =
      ExtensionApiFrameIdMap::GetDocumentLifecycle(frame_host);
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

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
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

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
  details.document_id =
      ExtensionApiFrameIdMap::GetDocumentId(frame_host).ToString();
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          frame_host->GetParentOrOuterDocument()) {
    details.parent_document_id =
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString();
  }
  details.frame_type = ExtensionApiFrameIdMap::GetFrameType(frame_host);
  details.document_lifecycle =
      ExtensionApiFrameIdMap::GetDocumentLifecycle(frame_host);
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

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
  details.document_id =
      ExtensionApiFrameIdMap::GetDocumentId(navigation_handle).ToString();
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          navigation_handle->GetParentFrameOrOuterDocument()) {
    details.parent_document_id =
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString();
  }
  details.frame_type = ExtensionApiFrameIdMap::GetFrameType(navigation_handle);
  details.document_lifecycle =
      ExtensionApiFrameIdMap::GetDocumentLifecycle(navigation_handle);
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

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
  details.time_stamp = base::Time::Now().InMillisecondsFSinceUnixEpoch();

  auto event = std::make_unique<Event>(
      events::WEB_NAVIGATION_ON_TAB_REPLACED,
      web_navigation::OnTabReplaced::kEventName,
      web_navigation::OnTabReplaced::Create(details), browser_context);
  DispatchEvent(browser_context, std::move(event), GURL());
}

}  // namespace web_navigation_api_helpers

}  // namespace extensions

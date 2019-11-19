// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_constants.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/api/web_navigation.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/view_type_utils.h"
#include "net/base/net_errors.h"

namespace GetFrame = extensions::api::web_navigation::GetFrame;
namespace GetAllFrames = extensions::api::web_navigation::GetAllFrames;

namespace extensions {

namespace web_navigation = api::web_navigation;

namespace {

using TabObserverMap =
    std::map<content::WebContents*, WebNavigationTabObserver*>;

TabObserverMap& GetTabObserverMap() {
  static base::NoDestructor<TabObserverMap> s;
  return *s;
}

}  // namespace

// WebNavigtionEventRouter -------------------------------------------

WebNavigationEventRouter::PendingWebContents::PendingWebContents() = default;
WebNavigationEventRouter::PendingWebContents::~PendingWebContents() {}

void WebNavigationEventRouter::PendingWebContents::Set(
    int source_tab_id,
    int source_render_process_id,
    int source_extension_frame_id,
    content::WebContents* target_web_contents,
    const GURL& target_url,
    base::OnceCallback<void(content::WebContents*)> on_destroy) {
  Observe(target_web_contents);
  source_tab_id_ = source_tab_id;
  source_render_process_id_ = source_render_process_id;
  source_extension_frame_id_ = source_extension_frame_id;
  target_web_contents_ = target_web_contents;
  target_url_ = target_url;
  on_destroy_ = std::move(on_destroy);
}

void WebNavigationEventRouter::PendingWebContents::WebContentsDestroyed() {
  std::move(on_destroy_).Run(target_web_contents_);
  // |this| is deleted!
}

WebNavigationEventRouter::WebNavigationEventRouter(Profile* profile)
    : profile_(profile), browser_tab_strip_tracker_(this, this, nullptr) {
  browser_tab_strip_tracker_.Init();
}

WebNavigationEventRouter::~WebNavigationEventRouter() = default;

bool WebNavigationEventRouter::ShouldTrackBrowser(Browser* browser) {
  return profile_->IsSameProfile(browser->profile());
}

void WebNavigationEventRouter::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    WebNavigationTabObserver* tab_observer =
        WebNavigationTabObserver::Get(replace->old_contents);

    if (!tab_observer) {
      // If you hit this DCHECK(), please add reproduction steps to
      // http://crbug.com/109464.
      DCHECK(GetViewType(replace->old_contents) != VIEW_TYPE_TAB_CONTENTS);
      return;
    }
    if (!FrameNavigationState::IsValidUrl(replace->old_contents->GetURL()) ||
        !FrameNavigationState::IsValidUrl(replace->new_contents->GetURL()))
      return;

    web_navigation_api_helpers::DispatchOnTabReplaced(
        replace->old_contents, profile_, replace->new_contents);
  } else if (change.type() == TabStripModelChange::kInserted) {
    for (auto& tab : change.GetInsert()->contents)
      TabAdded(tab.contents);
  }
}

void WebNavigationEventRouter::RecordNewWebContents(
    content::WebContents* source_web_contents,
    int source_render_process_id,
    int source_render_frame_id,
    GURL target_url,
    content::WebContents* target_web_contents,
    bool not_yet_in_tabstrip) {
  if (source_render_frame_id == 0)
    return;
  WebNavigationTabObserver* tab_observer =
      WebNavigationTabObserver::Get(source_web_contents);
  if (!tab_observer) {
    // If you hit this DCHECK(), please add reproduction steps to
    // http://crbug.com/109464.
    DCHECK(GetViewType(source_web_contents) != VIEW_TYPE_TAB_CONTENTS);
    return;
  }
  const FrameNavigationState& frame_navigation_state =
      tab_observer->frame_navigation_state();

  content::RenderFrameHost* frame_host = content::RenderFrameHost::FromID(
      source_render_process_id, source_render_frame_id);
  if (!frame_navigation_state.CanSendEvents(frame_host))
    return;

  int source_extension_frame_id =
      ExtensionApiFrameIdMap::GetFrameId(frame_host);
  int source_tab_id = ExtensionTabUtil::GetTabId(source_web_contents);

  // If the WebContents isn't yet inserted into a tab strip, we need to delay
  // the extension event until the WebContents is fully initialized.
  if (not_yet_in_tabstrip) {
    pending_web_contents_[target_web_contents].Set(
        source_tab_id, source_render_process_id, source_extension_frame_id,
        target_web_contents, target_url,
        base::BindOnce(&WebNavigationEventRouter::PendingWebContentsDestroyed,
                       base::Unretained(this)));
  } else {
    web_navigation_api_helpers::DispatchOnCreatedNavigationTarget(
        source_tab_id, source_render_process_id, source_extension_frame_id,
        target_web_contents->GetBrowserContext(), target_web_contents,
        target_url);
  }
}

void WebNavigationEventRouter::TabAdded(content::WebContents* tab) {
  auto iter = pending_web_contents_.find(tab);
  if (iter == pending_web_contents_.end())
    return;

  const PendingWebContents& pending_tab = iter->second;
  web_navigation_api_helpers::DispatchOnCreatedNavigationTarget(
      pending_tab.source_tab_id(), pending_tab.source_render_process_id(),
      pending_tab.source_extension_frame_id(),
      pending_tab.target_web_contents()->GetBrowserContext(),
      pending_tab.target_web_contents(), pending_tab.target_url());
  pending_web_contents_.erase(iter);
}

void WebNavigationEventRouter::PendingWebContentsDestroyed(
    content::WebContents* tab) {
  pending_web_contents_.erase(tab);
}

// WebNavigationTabObserver ------------------------------------------

WebNavigationTabObserver::WebNavigationTabObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  GetTabObserverMap().insert(TabObserverMap::value_type(web_contents, this));
  navigation_state_.FrameHostCreated(web_contents->GetMainFrame());
}

WebNavigationTabObserver::~WebNavigationTabObserver() {}

// static
WebNavigationTabObserver* WebNavigationTabObserver::Get(
    content::WebContents* web_contents) {
  auto i = GetTabObserverMap().find(web_contents);
  return i == GetTabObserverMap().end() ? NULL : i->second;
}

void WebNavigationTabObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (navigation_state_.CanSendEvents(render_frame_host) &&
      !navigation_state_.GetDocumentLoadCompleted(render_frame_host)) {
    web_navigation_api_helpers::DispatchOnErrorOccurred(
        web_contents(), render_frame_host,
        navigation_state_.GetUrl(render_frame_host), net::ERR_ABORTED);
    navigation_state_.SetErrorOccurredInFrame(render_frame_host);
  }
}

void WebNavigationTabObserver::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  navigation_state_.FrameHostDeleted(render_frame_host);
}

void WebNavigationTabObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (old_host)
    RenderFrameHostPendingDeletion(old_host);
  navigation_state_.FrameHostCreated(new_host);
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
  if (!navigation_state_.CanSendEvents(render_frame_host))
    return;

  navigation_state_.SetParsingFinished(render_frame_host);
  web_navigation_api_helpers::DispatchOnDOMContentLoaded(
      web_contents(), render_frame_host,
      navigation_state_.GetUrl(render_frame_host));

  if (!navigation_state_.GetDocumentLoadCompleted(render_frame_host))
    return;

  // The load might already have finished by the time we finished parsing. For
  // compatibility reasons, we artifically delay the load completed signal until
  // after parsing was completed.
  web_navigation_api_helpers::DispatchOnCompleted(
      web_contents(), render_frame_host,
      navigation_state_.GetUrl(render_frame_host));
}

void WebNavigationTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // When showing replacement content, we might get load signals for frames
  // that weren't reguarly loaded.
  if (!navigation_state_.IsValidFrame(render_frame_host))
    return;

  navigation_state_.SetDocumentLoadCompleted(render_frame_host);
  if (!navigation_state_.CanSendEvents(render_frame_host))
    return;

  // A new navigation might have started before the old one completed.
  // Ignore the old navigation completion in that case.
  if (navigation_state_.GetUrl(render_frame_host) != validated_url)
    return;

  // The load might already have finished by the time we finished parsing. For
  // compatibility reasons, we artifically delay the load completed signal until
  // after parsing was completed.
  if (!navigation_state_.GetParsingFinished(render_frame_host))
    return;
  web_navigation_api_helpers::DispatchOnCompleted(
      web_contents(), render_frame_host,
      navigation_state_.GetUrl(render_frame_host));
}

void WebNavigationTabObserver::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  // When showing replacement content, we might get load signals for frames
  // that weren't reguarly loaded.
  if (!navigation_state_.IsValidFrame(render_frame_host))
    return;

  if (navigation_state_.CanSendEvents(render_frame_host)) {
    web_navigation_api_helpers::DispatchOnErrorOccurred(
        web_contents(), render_frame_host,
        navigation_state_.GetUrl(render_frame_host), error_code);
  }
  navigation_state_.SetErrorOccurredInFrame(render_frame_host);
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
  if (!navigation_state_.CanSendEvents(source_render_frame_host))
    return;

  // We only send the onCreatedNavigationTarget if we end up creating a new
  // window.
  if (disposition != WindowOpenDisposition::SINGLETON_TAB &&
      disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_POPUP &&
      disposition != WindowOpenDisposition::NEW_WINDOW &&
      disposition != WindowOpenDisposition::OFF_THE_RECORD)
    return;

  WebNavigationAPI* api = WebNavigationAPI::GetFactoryInstance()->Get(
      web_contents()->GetBrowserContext());
  if (!api)
    return;  // Possible in unit tests.
  WebNavigationEventRouter* router = api->web_navigation_event_router_.get();
  if (!router)
    return;

  TabStripModel* ignored_tab_strip_model = nullptr;
  int ignored_tab_index = -1;
  bool new_contents_is_present_in_tabstrip = ExtensionTabUtil::GetTabStripModel(
      new_contents, &ignored_tab_strip_model, &ignored_tab_index);
  router->RecordNewWebContents(
      web_contents(), source_render_frame_host->GetProcess()->GetID(),
      source_render_frame_host->GetRoutingID(), url, new_contents,
      !new_contents_is_present_in_tabstrip);
}

void WebNavigationTabObserver::WebContentsDestroyed() {
  GetTabObserverMap().erase(web_contents());
}

void WebNavigationTabObserver::DispatchCachedOnBeforeNavigate() {
  if (!pending_on_before_navigate_event_)
    return;

  // EventRouter can be null in unit tests.
  EventRouter* event_router =
      EventRouter::Get(web_contents()->GetBrowserContext());
  if (event_router)
    event_router->BroadcastEvent(std::move(pending_on_before_navigate_event_));
}

void WebNavigationTabObserver::HandleCommit(
    content::NavigationHandle* navigation_handle) {
  bool is_reference_fragment_navigation =
      navigation_handle->IsSameDocument() &&
      IsReferenceFragmentNavigation(navigation_handle->GetRenderFrameHost(),
                                    navigation_handle->GetURL());

  navigation_state_.StartTrackingDocumentLoad(
      navigation_handle->GetRenderFrameHost(), navigation_handle->GetURL(),
      navigation_handle->IsSameDocument(),
      false);  // is_error_page

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
}

void WebNavigationTabObserver::HandleError(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted()) {
    navigation_state_.StartTrackingDocumentLoad(
        navigation_handle->GetRenderFrameHost(), navigation_handle->GetURL(),
        navigation_handle->IsSameDocument(),
        true);  // is_error_page
  }

  web_navigation_api_helpers::DispatchOnErrorOccurred(navigation_handle);
}

// See also NavigationController::IsURLSameDocumentNavigation.
bool WebNavigationTabObserver::IsReferenceFragmentNavigation(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  GURL existing_url = navigation_state_.GetUrl(render_frame_host);
  if (existing_url == url)
    return false;

  url::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
      url.ReplaceComponents(replacements);
}

void WebNavigationTabObserver::RenderFrameHostPendingDeletion(
    content::RenderFrameHost* pending_delete_rfh) {
  // The |pending_delete_rfh| and its children are now pending deletion.
  // Stop tracking them.

  // 1) Collect them.
  std::vector<content::RenderFrameHost*> to_be_deleted;
  for (content::RenderFrameHost* render_frame_host : navigation_state_) {
    if (render_frame_host == pending_delete_rfh ||
        render_frame_host->IsDescendantOf(pending_delete_rfh)) {
      to_be_deleted.push_back(render_frame_host);
    }
  }

  // 2) Delete them.
  for (content::RenderFrameHost* render_frame_host : to_be_deleted) {
    // The RenderFrame may still be loading. Call RenderFrameDeleted()
    // immediately to properly dispatch a load error occurred.
    RenderFrameDeleted(render_frame_host);

    navigation_state_.FrameHostDeleted(render_frame_host);
  }
}

ExtensionFunction::ResponseAction WebNavigationGetFrameFunction::Run() {
  std::unique_ptr<GetFrame::Params> params(GetFrame::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->details.tab_id;
  int frame_id = params->details.frame_id;

  content::WebContents* web_contents;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                    include_incognito_information(),
                                    &web_contents) ||
      !web_contents) {
    return RespondNow(OneArgument(std::make_unique<base::Value>()));
  }

  WebNavigationTabObserver* observer =
      WebNavigationTabObserver::Get(web_contents);
  DCHECK(observer);

  const FrameNavigationState& frame_navigation_state =
      observer->frame_navigation_state();

  content::RenderFrameHost* render_frame_host =
      ExtensionApiFrameIdMap::Get()->GetRenderFrameHostById(web_contents,
                                                            frame_id);
  if (!frame_navigation_state.IsValidFrame(render_frame_host))
    return RespondNow(OneArgument(std::make_unique<base::Value>()));

  GURL frame_url = frame_navigation_state.GetUrl(render_frame_host);
  if (!frame_navigation_state.IsValidUrl(frame_url))
    return RespondNow(OneArgument(std::make_unique<base::Value>()));

  GetFrame::Results::Details frame_details;
  frame_details.url = frame_url.spec();
  frame_details.error_occurred =
      frame_navigation_state.GetErrorOccurredInFrame(render_frame_host);
  frame_details.parent_frame_id =
      ExtensionApiFrameIdMap::GetFrameId(render_frame_host->GetParent());
  return RespondNow(ArgumentList(GetFrame::Results::Create(frame_details)));
}

ExtensionFunction::ResponseAction WebNavigationGetAllFramesFunction::Run() {
  std::unique_ptr<GetAllFrames::Params> params(
      GetAllFrames::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->details.tab_id;

  content::WebContents* web_contents;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                    include_incognito_information(),
                                    &web_contents) ||
      !web_contents) {
    return RespondNow(OneArgument(std::make_unique<base::Value>()));
  }

  WebNavigationTabObserver* observer =
      WebNavigationTabObserver::Get(web_contents);
  DCHECK(observer);

  const FrameNavigationState& navigation_state =
      observer->frame_navigation_state();

  std::vector<GetAllFrames::Results::DetailsType> result_list;
  for (auto it = navigation_state.begin(); it != navigation_state.end(); ++it) {
    GURL frame_url = navigation_state.GetUrl(*it);
    if (!navigation_state.IsValidUrl(frame_url))
      continue;
    GetAllFrames::Results::DetailsType frame;
    frame.url = frame_url.spec();
    frame.frame_id = ExtensionApiFrameIdMap::GetFrameId(*it);
    frame.parent_frame_id =
        ExtensionApiFrameIdMap::GetFrameId((*it)->GetParent());
    frame.process_id = (*it)->GetProcess()->GetID();
    frame.error_occurred = navigation_state.GetErrorOccurredInFrame(*it);
    result_list.push_back(std::move(frame));
  }
  return RespondNow(ArgumentList(GetAllFrames::Results::Create(result_list)));
}

WebNavigationAPI::WebNavigationAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 web_navigation::OnBeforeNavigate::kEventName);
  event_router->RegisterObserver(this, web_navigation::OnCommitted::kEventName);
  event_router->RegisterObserver(this, web_navigation::OnCompleted::kEventName);
  event_router->RegisterObserver(
      this, web_navigation::OnCreatedNavigationTarget::kEventName);
  event_router->RegisterObserver(
      this, web_navigation::OnDOMContentLoaded::kEventName);
  event_router->RegisterObserver(
      this, web_navigation::OnHistoryStateUpdated::kEventName);
  event_router->RegisterObserver(this,
                                 web_navigation::OnErrorOccurred::kEventName);
  event_router->RegisterObserver(
      this, web_navigation::OnReferenceFragmentUpdated::kEventName);
  event_router->RegisterObserver(this,
                                 web_navigation::OnTabReplaced::kEventName);
}

WebNavigationAPI::~WebNavigationAPI() {
}

void WebNavigationAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<WebNavigationAPI>>::
    DestructorAtExit g_web_navigation_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<WebNavigationAPI>*
WebNavigationAPI::GetFactoryInstance() {
  return g_web_navigation_api_factory.Pointer();
}

void WebNavigationAPI::OnListenerAdded(const EventListenerInfo& details) {
  web_navigation_event_router_.reset(new WebNavigationEventRouter(
      Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebNavigationTabObserver)

}  // namespace extensions

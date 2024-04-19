// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"

#include <memory>

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
#include "extensions/common/mojom/view_type.mojom.h"
#include "net/base/net_errors.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/pdf_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace GetFrame = extensions::api::web_navigation::GetFrame;
namespace GetAllFrames = extensions::api::web_navigation::GetAllFrames;

namespace extensions {

namespace web_navigation = api::web_navigation;

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
  std::move(on_destroy_).Run(target_web_contents_.get());
  // |this| is deleted!
}

WebNavigationEventRouter::WebNavigationEventRouter(Profile* profile)
    : profile_(profile), browser_tab_strip_tracker_(this, this) {
  browser_tab_strip_tracker_.Init();
}

WebNavigationEventRouter::~WebNavigationEventRouter() = default;

bool WebNavigationEventRouter::ShouldTrackBrowser(Browser* browser) {
  return profile_->IsSameOrParent(browser->profile());
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
      DCHECK(GetViewType(replace->old_contents) !=
             mojom::ViewType::kTabContents);
      return;
    }
    if (!FrameNavigationState::IsValidUrl(
            replace->old_contents->GetLastCommittedURL()) ||
        !FrameNavigationState::IsValidUrl(
            replace->new_contents->GetLastCommittedURL()))
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
    DCHECK(GetViewType(source_web_contents) != mojom::ViewType::kTabContents);
    return;
  }

  auto* frame_host = content::RenderFrameHost::FromID(source_render_process_id,
                                                      source_render_frame_id);
  auto* frame_navigation_state =
      FrameNavigationState::GetForCurrentDocument(frame_host);

  if (!frame_navigation_state || !frame_navigation_state->CanSendEvents())
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
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<WebNavigationTabObserver>(*web_contents) {}

WebNavigationTabObserver::~WebNavigationTabObserver() {}

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
  if (old_host)
    RenderFrameHostPendingDeletion(old_host);
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
  if (!navigation_state || !navigation_state->CanSendEvents())
    return;

  navigation_state->SetParsingFinished();
  web_navigation_api_helpers::DispatchOnDOMContentLoaded(
      web_contents(), render_frame_host, navigation_state->GetUrl());

  if (!navigation_state->GetDocumentLoadCompleted())
    return;

  // The load might already have finished by the time we finished parsing. For
  // compatibility reasons, we artifically delay the load completed signal until
  // after parsing was completed.
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
  if (!navigation_state)
    return;

  navigation_state->SetDocumentLoadCompleted();
  if (!navigation_state->CanSendEvents())
    return;

  // A new navigation might have started before the old one completed.
  // Ignore the old navigation completion in that case.
  if (navigation_state->GetUrl() != validated_url)
    return;

  // The load might already have finished by the time we finished parsing. For
  // compatibility reasons, we artifically delay the load completed signal until
  // after parsing was completed.
  if (!navigation_state->GetParsingFinished())
    return;
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
  if (!navigation_state)
    return;

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
  if (!navigation_state || !navigation_state->CanSendEvents())
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
  if (existing_url == url)
    return false;

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

ExtensionFunction::ResponseAction WebNavigationGetFrameFunction::Run() {
  std::optional<GetFrame::Params> params = GetFrame::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = api::tabs::TAB_ID_NONE;
  int frame_id = -1;

  content::RenderFrameHost* render_frame_host = nullptr;
  if (params->details.document_id) {
    ExtensionApiFrameIdMap::DocumentId document_id =
        ExtensionApiFrameIdMap::DocumentIdFromString(
            *params->details.document_id);
    if (!document_id)
      return RespondNow(Error("Invalid documentId."));

    // Note that we will globally find a RenderFrameHost but validate that
    // we are in the right context still as we may be in the wrong profile
    // or in incognito mode.
    render_frame_host =
        ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
            document_id);

    if (!render_frame_host)
      return RespondNow(WithArguments(base::Value()));

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    // We found the RenderFrameHost through a generic lookup so we must test to
    // see if the WebContents is actually in our BrowserContext.
    if (!ExtensionTabUtil::IsWebContentsInContext(
            web_contents, browser_context(), include_incognito_information())) {
      return RespondNow(WithArguments(base::Value()));
    }

    tab_id = ExtensionTabUtil::GetTabId(web_contents);
    frame_id = ExtensionApiFrameIdMap::GetFrameId(render_frame_host);

    // If the provided tab_id and frame_id do not match the calculated ones
    // return.
    if ((params->details.tab_id && *params->details.tab_id != tab_id) ||
        (params->details.frame_id && *params->details.frame_id != frame_id)) {
      return RespondNow(WithArguments(base::Value()));
    }
  } else {
    // If documentId is not provided, tab_id and frame_id must be. Return early
    // if not.
    if (!params->details.tab_id || !params->details.frame_id) {
      return RespondNow(Error(
          "Either documentId or both tabId and frameId must be specified."));
    }

    tab_id = *params->details.tab_id;
    frame_id = *params->details.frame_id;

    content::WebContents* web_contents = nullptr;
    if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                      include_incognito_information(),
                                      &web_contents) ||
        !web_contents) {
      return RespondNow(WithArguments(base::Value()));
    }

    render_frame_host = ExtensionApiFrameIdMap::Get()->GetRenderFrameHostById(
        web_contents, frame_id);
  }

  auto* frame_navigation_state =
      render_frame_host
          ? FrameNavigationState::GetForCurrentDocument(render_frame_host)
          : nullptr;
  if (!frame_navigation_state)
    return RespondNow(WithArguments(base::Value()));

  GURL frame_url = frame_navigation_state->GetUrl();
  if (!FrameNavigationState::IsValidUrl(frame_url))
    return RespondNow(WithArguments(base::Value()));

  GetFrame::Results::Details frame_details;
  frame_details.url = frame_url.spec();
  frame_details.error_occurred =
      frame_navigation_state->GetErrorOccurredInFrame();
  frame_details.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(render_frame_host);
  frame_details.document_id =
      ExtensionApiFrameIdMap::GetDocumentId(render_frame_host).ToString();
  // Only set the parentDocumentId value if we have a parent.
  if (content::RenderFrameHost* parent_frame_host =
          render_frame_host->GetParentOrOuterDocument()) {
    frame_details.parent_document_id =
        ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host).ToString();
  }
  frame_details.frame_type =
      ExtensionApiFrameIdMap::GetFrameType(render_frame_host);
  frame_details.document_lifecycle =
      ExtensionApiFrameIdMap::GetDocumentLifecycle(render_frame_host);

  return RespondNow(ArgumentList(GetFrame::Results::Create(frame_details)));
}

ExtensionFunction::ResponseAction WebNavigationGetAllFramesFunction::Run() {
  std::optional<GetAllFrames::Params> params =
      GetAllFrames::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int tab_id = params->details.tab_id;

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                    include_incognito_information(),
                                    &web_contents) ||
      !web_contents) {
    return RespondNow(WithArguments(base::Value()));
  }

  std::vector<GetAllFrames::Results::DetailsType> result_list;

  // We currently do not expose back/forward cached frames in the GetAllFrames
  // API, but we do explicitly include prerendered frames.
  web_contents
      ->ForEachRenderFrameHostWithAction(
          [web_contents,
           &result_list](content::RenderFrameHost* render_frame_host) {
            // Don't expose inner WebContents for the getFrames API.
            if (content::WebContents::FromRenderFrameHost(render_frame_host) !=
                web_contents) {
              return content::RenderFrameHost::FrameIterationAction::
                  kSkipChildren;
            }

#if BUILDFLAG(ENABLE_PDF)
            if (chrome_pdf::features::IsOopifPdfEnabled()) {
              // Don't expose any child frames of the PDF extension frame, such
              // as the PDF content frame.
              content::RenderFrameHost* parent = render_frame_host->GetParent();
              if (parent &&
                  IsPdfExtensionOrigin(parent->GetLastCommittedOrigin())) {
                return content::RenderFrameHost::FrameIterationAction::
                    kSkipChildren;
              }
            }
#endif  // BUILDFLAG(ENABLE_PDF)

            auto* navigation_state =
                FrameNavigationState::GetForCurrentDocument(render_frame_host);

            if (!navigation_state ||
                !FrameNavigationState::IsValidUrl(navigation_state->GetUrl())) {
              return content::RenderFrameHost::FrameIterationAction::kContinue;
            }

            // Skip back/forward cached frames.
            if (render_frame_host->IsInLifecycleState(
                    content::RenderFrameHost::LifecycleState::
                        kInBackForwardCache)) {
              return content::RenderFrameHost::FrameIterationAction::
                  kSkipChildren;
            }

            GetAllFrames::Results::DetailsType frame;
            frame.url = navigation_state->GetUrl().spec();
            frame.frame_id =
                ExtensionApiFrameIdMap::GetFrameId(render_frame_host);
            frame.parent_frame_id =
                ExtensionApiFrameIdMap::GetParentFrameId(render_frame_host);
            frame.document_id =
                ExtensionApiFrameIdMap::GetDocumentId(render_frame_host)
                    .ToString();
            // Only set the parentDocumentId value if we have a parent.
            if (content::RenderFrameHost* parent_frame_host =
                    render_frame_host->GetParentOrOuterDocument()) {
              frame.parent_document_id =
                  ExtensionApiFrameIdMap::GetDocumentId(parent_frame_host)
                      .ToString();
            }
            frame.frame_type =
                ExtensionApiFrameIdMap::GetFrameType(render_frame_host);
            frame.document_lifecycle =
                ExtensionApiFrameIdMap::GetDocumentLifecycle(render_frame_host);
            frame.process_id = render_frame_host->GetProcess()->GetID();
            frame.error_occurred = navigation_state->GetErrorOccurredInFrame();
            result_list.push_back(std::move(frame));
            return content::RenderFrameHost::FrameIterationAction::kContinue;
          });

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

WebNavigationAPI::~WebNavigationAPI() {}

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
  web_navigation_event_router_ = std::make_unique<WebNavigationEventRouter>(
      Profile::FromBrowserContext(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebNavigationTabObserver);

}  // namespace extensions

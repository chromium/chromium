// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions WebNavigation API.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"

#include <memory>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_constants.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/web_navigation.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/pdf_util.h"  // nogncheck
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace GetFrame = extensions::api::web_navigation::GetFrame;
namespace GetAllFrames = extensions::api::web_navigation::GetAllFrames;

namespace extensions {

namespace web_navigation = api::web_navigation;

// API functions -----------------------------------------------------

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
            frame.process_id =
                render_frame_host->GetProcess()->GetDeprecatedID();
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

WebNavigationAPI::~WebNavigationAPI() = default;

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

}  // namespace extensions

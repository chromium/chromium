// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "components/enterprise/connectors/core/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_connectors {

namespace {

bool IsExtensionOrigin(const std::optional<url::Origin>& origin) {
  return origin.has_value() && origin->scheme() == extensions::kExtensionScheme;
}

// Returns true if `command` initiates a top-level tab navigation. CDP commands
// are defined in `third_party/blink/public/devtools_protocol/domains/Page.pdl`.
bool IsNavigation(std::string_view command) {
  return command == extensions::TabsCreateFunction::static_function_name() ||
         command == extensions::TabsUpdateFunction::static_function_name() ||
         command == "Page.navigate" || command == "Page.reload";
}

// Returns true if `command` can actuate a tab and should be tracked. CDP
// commands and domains are defined in `v8/include/js_protocol.pdl` and
// `third_party/blink/public/devtools_protocol/domains/`.
bool IsTracked(std::string_view command) {
  return IsNavigation(command) || command == "Runtime.evaluate" ||
         command == "Runtime.callFunctionOn" || command.starts_with("Input.") ||
         command == "DOM.focus" || command.starts_with("DOM.set");
}

}  // namespace

// static
bool ExtensionActorTracker::IsEnabled(
    content::BrowserContext* browser_context) {
  if (!browser_context ||
      !base::FeatureList::IsEnabled(kEnterpriseExtensionActorAttribution)) {
    return false;
  }
  auto* service =
      ConnectorsServiceFactory::GetForBrowserContext(browser_context);
  return service && service->GetReportingSettings().has_value();
}

ExtensionActorTracker::ExtensionActorTracker(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ExtensionActorTracker>(*web_contents) {}

ExtensionActorTracker::~ExtensionActorTracker() = default;

// static
void ExtensionActorTracker::SetActiveActorInternal(
    content::WebContents* web_contents,
    const extensions::ExtensionId& extension_id,
    bool is_initial_navigation) {
  if (!web_contents || extension_id.empty() ||
      !IsEnabled(web_contents->GetBrowserContext())) {
    return;
  }
  auto* tracker = GetOrCreateForWebContents(web_contents);
  tracker->active_actor_id_ = extension_id;
  tracker->is_initial_navigation_ |= is_initial_navigation;
}

// static
void ExtensionActorTracker::SetActiveActor(
    content::WebContents* web_contents,
    const extensions::ExtensionId& extension_id,
    std::string_view command) {
  if (!web_contents || !IsTracked(command)) {
    return;
  }
  bool is_initial_navigation =
      IsNavigation(command) && !web_contents->GetVisibleURL().IsAboutBlank();
  SetActiveActorInternal(web_contents, extension_id, is_initial_navigation);
}

// static
void ExtensionActorTracker::ClearActiveActor(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  if (auto* tracker = FromWebContents(web_contents)) {
    tracker->active_actor_id_.reset();
    tracker->is_initial_navigation_ = false;
  }
}

// static
std::optional<extensions::ExtensionId> ExtensionActorTracker::GetActiveActor(
    content::WebContents* web_contents) {
  if (!web_contents || !IsEnabled(web_contents->GetBrowserContext())) {
    return std::nullopt;
  }
  auto* tracker = FromWebContents(web_contents);
  return tracker ? tracker->active_actor_id_ : std::nullopt;
}

void ExtensionActorTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle || !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->GetURL().IsAboutBlank()) {
    return;
  }

  bool is_initial_navigation = std::exchange(is_initial_navigation_, false);

  if (IsExtensionOrigin(navigation_handle->GetInitiatorOrigin())) {
    active_actor_id_ = navigation_handle->GetInitiatorOrigin()->host();
    return;
  }

  // Preserve attribution across the initial extension navigation, uncommitted
  // or same-document navigations, and form submissions.
  if (is_initial_navigation || !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      (active_actor_id_.has_value() && navigation_handle->IsFormSubmission())) {
    return;
  }

  active_actor_id_.reset();
}

void ExtensionActorTracker::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  // Ignore synthetic inputs from `chrome.debugger` and passive scroll gestures.
  if ((event.GetModifiers() & blink::WebInputEvent::kFromDebugger) ||
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    return;
  }
  active_actor_id_.reset();
  is_initial_navigation_ = false;
}

void ExtensionActorTracker::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (!new_contents) {
    return;
  }

  // Propagate attribution to child tabs/popups (`window.open`), or clear any
  // stale attribution.
  if (active_actor_id_.has_value()) {
    SetActiveActorInternal(new_contents, *active_actor_id_,
                           /*is_initial_navigation=*/true);
  } else {
    ClearActiveActor(new_contents);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExtensionActorTracker);

}  // namespace enterprise_connectors

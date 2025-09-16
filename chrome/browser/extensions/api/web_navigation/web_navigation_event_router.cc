// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_event_router.h"

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

WebNavigationEventRouter::PendingWebContents::PendingWebContents() = default;
WebNavigationEventRouter::PendingWebContents::~PendingWebContents() = default;

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

bool WebNavigationEventRouter::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return profile_->IsSameOrParent(browser->GetProfile());
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
            replace->new_contents->GetLastCommittedURL())) {
      return;
    }

    web_navigation_api_helpers::DispatchOnTabReplaced(
        replace->old_contents, profile_, replace->new_contents);
  } else if (change.type() == TabStripModelChange::kInserted) {
    for (auto& tab : change.GetInsert()->contents) {
      TabAdded(tab.contents);
    }
  }
}

void WebNavigationEventRouter::RecordNewWebContents(
    content::WebContents* source_web_contents,
    int source_render_process_id,
    int source_render_frame_id,
    GURL target_url,
    content::WebContents* target_web_contents,
    bool not_yet_in_tabstrip) {
  if (source_render_frame_id == 0) {
    return;
  }
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

  if (!frame_navigation_state || !frame_navigation_state->CanSendEvents()) {
    return;
  }

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
  if (iter == pending_web_contents_.end()) {
    return;
  }

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

}  // namespace extensions

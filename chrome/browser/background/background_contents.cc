// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents.h"

#include <utility>

#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/extension_host_queue.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/gfx/geometry/rect.h"

using content::SiteInstance;
using content::WebContents;

BackgroundContents::BackgroundContents(
    scoped_refptr<SiteInstance> site_instance,
    content::RenderFrameHost* opener,
    bool is_new_browsing_instance,
    Delegate* delegate,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace)
    : delegate_(delegate),
      extension_host_delegate_(extensions::ExtensionsBrowserClient::Get()
                                   ->CreateExtensionHostDelegate()) {
  profile_ = Profile::FromBrowserContext(
      site_instance->GetBrowserContext());

  WebContents::CreateParams create_params(profile_, std::move(site_instance));
  create_params.opener_render_process_id =
      opener ? opener->GetProcess()->GetID() : MSG_ROUTING_NONE;
  create_params.opener_render_frame_id =
      opener ? opener->GetRoutingID() : MSG_ROUTING_NONE;

  if (session_storage_namespace) {
    content::SessionStorageNamespaceMap session_storage_namespace_map;
    session_storage_namespace_map.insert(
        std::make_pair(partition_config, session_storage_namespace));
    web_contents_ = WebContents::CreateWithSessionStorage(
        create_params, session_storage_namespace_map);
  } else {
    web_contents_ = WebContents::Create(create_params);
  }
  web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  extensions::SetViewType(web_contents_.get(),
                          extensions::mojom::ViewType::kBackgroundContents);
  web_contents_->SetDelegate(this);
  content::WebContentsObserver::Observe(web_contents_.get());

  // Add the TaskManager-specific tag for the BackgroundContents.
  task_manager::WebContentsTags::CreateForBackgroundContents(
      web_contents_.get(), this);
}

// Exposed to allow creating mocks.
BackgroundContents::BackgroundContents() = default;

BackgroundContents::~BackgroundContents() {
  if (!web_contents_.get())   // Will be null for unit tests.
    return;

  extensions::ExtensionHostQueue::GetInstance().Remove(this);
}

const GURL& BackgroundContents::GetURL() const {
  return web_contents_.get() ? web_contents_->GetURL() : GURL::EmptyGURL();
}

void BackgroundContents::CreateRendererSoon(const GURL& url) {
  initial_url_ = url;
  extensions::ExtensionHostQueue::GetInstance().Add(this);
}

void BackgroundContents::CloseContents(WebContents* source) {
  delegate_->OnBackgroundContentsClosed(this);
  // |this| is deleted.
}

bool BackgroundContents::ShouldSuppressDialogs(WebContents* source) {
  return true;
}

void BackgroundContents::PrimaryPageChanged(content::Page& page) {
  // Note: because BackgroundContents are only available to extension apps,
  // navigation is limited to urls within the app's extent. This is enforced in
  // RenderView::decidePolicyForNavigation. If BackgroundContents become
  // available as a part of the web platform, it probably makes sense to have
  // some way to scope navigation of a background page to its opener's security
  // origin. Note: if the first navigation is to a URL outside the app's
  // extent a background page will be opened but will remain at about:blank.
  delegate_->OnBackgroundContentsNavigated(this);
}

// Forward requests to add a new WebContents to our delegate.
WebContents* BackgroundContents::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  delegate_->AddWebContents(std::move(new_contents), target_url, disposition,
                            window_features, was_blocked);
  return nullptr;
}

bool BackgroundContents::IsNeverComposited(content::WebContents* web_contents) {
  DCHECK_EQ(extensions::mojom::ViewType::kBackgroundContents,
            extensions::GetViewType(web_contents));
  return true;
}

void BackgroundContents::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  delegate_->OnBackgroundContentsTerminated(this);
  // |this| is deleted.
}

void BackgroundContents::CreateRendererNow() {
  base::WeakPtr<content::NavigationHandle> handle =
      web_contents()->GetController().LoadURL(initial_url_, content::Referrer(),
                                              ui::PAGE_TRANSITION_LINK,
                                              std::string());
  if (handle) {
    ukm::builders::Extensions_BackgroundContentsCreated(
        handle->GetNextPageUkmSourceId())
        .SetSeen(true)
        .Record(ukm::UkmRecorder::Get());
  }
}

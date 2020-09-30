// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::RenderViewHost;
using content::WebContents;
using content::WebContentsObserver;
using guest_view::GuestViewBase;

namespace extensions {

namespace {
std::string GetPartitionName(WebAuthFlow::Partition partition) {
  switch (partition) {
    case WebAuthFlow::LAUNCH_WEB_AUTH_FLOW:
      return "launchWebAuthFlow";
    case WebAuthFlow::GET_AUTH_TOKEN:
      return "getAuthFlow";
  }

  NOTREACHED() << "Unexpected partition value " << partition;
  return std::string();
}
}  // namespace

namespace identity_private = api::identity_private;

WebAuthFlow::WebAuthFlow(Delegate* delegate,
                         Profile* profile,
                         const GURL& provider_url,
                         Mode mode,
                         Partition partition)
    : delegate_(delegate),
      profile_(profile),
      provider_url_(provider_url),
      mode_(mode),
      partition_(partition),
      embedded_window_created_(false) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("identity", "WebAuthFlow", this);
}

WebAuthFlow::~WebAuthFlow() {
  DCHECK(!delegate_);

  // Stop listening to notifications first since some of the code
  // below may generate notifications.
  registrar_.RemoveAll();
  WebContentsObserver::Observe(nullptr);

  if (!app_window_key_.empty()) {
    AppWindowRegistry::Get(profile_)->RemoveObserver(this);

    if (app_window_ && app_window_->web_contents())
      app_window_->web_contents()->Close();
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("identity", "WebAuthFlow", this);
}

void WebAuthFlow::Start() {
  AppWindowRegistry::Get(profile_)->AddObserver(this);

  // Attach a random ID string to the window so we can recognize it
  // in OnAppWindowAdded.
  std::string random_bytes;
  crypto::RandBytes(base::WriteInto(&random_bytes, 33), 32);
  base::Base64Encode(random_bytes, &app_window_key_);

  // identityPrivate.onWebFlowRequest(app_window_key, provider_url_, mode_)
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(app_window_key_);
  args->AppendString(provider_url_.spec());
  if (mode_ == WebAuthFlow::INTERACTIVE)
    args->AppendString("interactive");
  else
    args->AppendString("silent");
  args->AppendString(GetPartitionName(partition_));

  auto event =
      std::make_unique<Event>(events::IDENTITY_PRIVATE_ON_WEB_FLOW_REQUEST,
                              identity_private::OnWebFlowRequest::kEventName,
                              std::move(args), profile_);
  ExtensionSystem* system = ExtensionSystem::Get(profile_);

  extensions::ComponentLoader* component_loader =
      system->extension_service()->component_loader();
  if (!component_loader->Exists(extension_misc::kIdentityApiUiAppId)) {
    component_loader->Add(
        IDR_IDENTITY_API_SCOPE_APPROVAL_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("identity_scope_approval_dialog")));
  }

  EventRouter::Get(profile_)->DispatchEventWithLazyListener(
      extension_misc::kIdentityApiUiAppId, std::move(event));
}

void WebAuthFlow::DetachDelegateAndDelete() {
  delegate_ = nullptr;
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

content::StoragePartition* WebAuthFlow::GetGuestPartition() {
  return content::BrowserContext::GetStoragePartition(
      profile_, GetWebViewPartitionConfig(partition_, profile_));
}

const std::string& WebAuthFlow::GetAppWindowKey() const {
  return app_window_key_;
}

// static
content::StoragePartitionConfig WebAuthFlow::GetWebViewPartitionConfig(
    Partition partition,
    content::BrowserContext* browser_context) {
  // This has to mirror the logic in WebViewGuest::CreateWebContents for
  // creating the correct StoragePartitionConfig.
  auto result = content::StoragePartitionConfig::Create(
      extension_misc::kIdentityApiUiAppId, GetPartitionName(partition),
      /*in_memory=*/true);
  result.set_fallback_to_partition_domain_for_blob_urls(
      browser_context->IsOffTheRecord()
          ? content::StoragePartitionConfig::FallbackMode::
                kFallbackPartitionInMemory
          : content::StoragePartitionConfig::FallbackMode::
                kFallbackPartitionOnDisk);
  return result;
}

void WebAuthFlow::OnAppWindowAdded(AppWindow* app_window) {
  if (app_window->window_key() == app_window_key_ &&
      app_window->extension_id() == extension_misc::kIdentityApiUiAppId) {
    app_window_ = app_window;
    WebContentsObserver::Observe(app_window->web_contents());

    registrar_.Add(
        this,
        content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
        content::NotificationService::AllBrowserContextsAndSources());
  }
}

void WebAuthFlow::OnAppWindowRemoved(AppWindow* app_window) {
  if (app_window->window_key() == app_window_key_ &&
      app_window->extension_id() == extension_misc::kIdentityApiUiAppId) {
    app_window_ = nullptr;
    registrar_.RemoveAll();
    WebContentsObserver::Observe(nullptr);

    if (delegate_)
      delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
  }
}

void WebAuthFlow::BeforeUrlLoaded(const GURL& url) {
  if (delegate_ && embedded_window_created_)
    delegate_->OnAuthFlowURLChange(url);
}

void WebAuthFlow::AfterUrlLoaded() {
  if (delegate_ && embedded_window_created_ && mode_ == WebAuthFlow::SILENT)
    delegate_->OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED);
}

void WebAuthFlow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED, type);
  DCHECK(app_window_);

  if (!delegate_ || embedded_window_created_)
    return;

  RenderViewHost* render_view(content::Details<RenderViewHost>(details).ptr());
  WebContents* web_contents = WebContents::FromRenderViewHost(render_view);
  GuestViewBase* guest = GuestViewBase::FromWebContents(web_contents);
  WebContents* owner = guest ? guest->owner_web_contents() : nullptr;
  if (!web_contents || owner != WebContentsObserver::web_contents())
    return;

  // Switch from watching the app window to the guest inside it.
  embedded_window_created_ = true;
  WebContentsObserver::Observe(web_contents);

  registrar_.RemoveAll();
}

void WebAuthFlow::RenderProcessGone(base::TerminationStatus status) {
  if (delegate_)
    delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
}

void WebAuthFlow::TitleWasSet(content::NavigationEntry* entry) {
  if (delegate_)
    delegate_->OnAuthFlowTitleChange(base::UTF16ToUTF8(entry->GetTitle()));
}

void WebAuthFlow::DidStopLoading() {
  AfterUrlLoaded();
}

void WebAuthFlow::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    BeforeUrlLoaded(navigation_handle->GetURL());
}

void WebAuthFlow::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  BeforeUrlLoaded(navigation_handle->GetURL());
}

void WebAuthFlow::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Websites may create and remove <iframe> during the auth flow. In
  // particular, to integrate CAPTCHA tests. Chrome shouldn't abort the auth
  // flow if a navigation failed in a sub-frame. https://crbug.com/1049565.
  if (!navigation_handle->IsInMainFrame())
    return;

  bool failed = false;
  if (navigation_handle->GetNetErrorCode() != net::OK) {
    if (navigation_handle->GetURL().spec() == url::kAboutBlankURL) {
      // As part of the OAUth 2.0 protocol with GAIA, at the end of the web
      // authorization flow, GAIA redirects to a custom scheme URL of type
      // |com.googleusercontent.apps.123:/<extension_id>|, where
      // |com.googleusercontent.apps.123| is the reverse DNS notation of the
      // client ID of the extension that started the web sign-in flow. (The
      // intent of this weird URL scheme was to make sure it couldn't be loaded
      // anywhere at all as this makes it much harder to pull off a cross-site
      // attack that could leak the returned oauth token to a malicious script
      // or site.)
      //
      // This URL is not an accessible URL from within a Guest WebView, so
      // during its load of this URL, Chrome changes it to |about:blank| and
      // then the Identity Scope Approval Dialog extension fails to load it.
      // Failing to load |about:blank| must not be treated as a failure of
      // the web auth flow.
      DCHECK_EQ(net::ERR_UNKNOWN_URL_SCHEME,
                navigation_handle->GetNetErrorCode());
    } else if (navigation_handle->GetResponseHeaders() &&
               navigation_handle->GetResponseHeaders()->response_code() ==
                   net::HTTP_NO_CONTENT) {
      // Navigation to no content URLs is aborted but shouldn't be treated as a
      // failure.
      // In particular, Gaia navigates to a no content page to pass Mirror
      // response headers.
    } else {
      failed = true;
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "identity", "DidFinishNavigationFailure", this, "error_code",
          navigation_handle->GetNetErrorCode());
    }
  } else if (navigation_handle->GetResponseHeaders() &&
             navigation_handle->GetResponseHeaders()->response_code() >= 400) {
    failed = true;
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
        "identity", "DidFinishNavigationFailure", this, "response_code",
        navigation_handle->GetResponseHeaders()->response_code());
  }

  if (failed && delegate_)
    delegate_->OnAuthFlowFailure(LOAD_FAILED);
}

}  // namespace extensions

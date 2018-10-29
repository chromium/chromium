// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_extensions_network_delegate.h"

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/permissions/api_permission.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
using extensions::ExtensionWebRequestEventRouter;

namespace {

enum RequestStatus { REQUEST_STARTED, REQUEST_DONE };

// for a particular RenderFrame.
void NotifyEPMRequestStatus(RequestStatus status,
                            void* profile_id,
                            uint64_t request_id,
                            int process_id,
                            int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile);
  DCHECK(process_manager);

  // Will be NULL if the request was not issued on behalf of a renderer (e.g. a
  // system-level request).
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(process_id, render_frame_id);
  if (render_frame_host) {
    if (status == REQUEST_STARTED) {
      process_manager->OnNetworkRequestStarted(render_frame_host, request_id);
    } else if (status == REQUEST_DONE) {
      process_manager->OnNetworkRequestDone(render_frame_host, request_id);
    } else {
      NOTREACHED();
    }
  }
}

void ForwardRequestStatus(
    RequestStatus status, net::URLRequest* request, void* profile_id) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return;

  if (status == REQUEST_STARTED && request->url_chain().size() > 1) {
    // It's a redirect, this request has already been counted.
    return;
  }

  int process_id, render_frame_id;
  if (info->GetAssociatedRenderFrame(&process_id, &render_frame_id)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyEPMRequestStatus, status, profile_id,
                       request->identifier(), process_id, render_frame_id));
  }
}

class ChromeExtensionsNetworkDelegateImpl
    : public ChromeExtensionsNetworkDelegate {
 public:
  explicit ChromeExtensionsNetworkDelegateImpl(
      extensions::EventRouterForwarder* event_router);
  ~ChromeExtensionsNetworkDelegateImpl() override;

 private:
  // ChromeExtensionsNetworkDelegate implementation.
  void ForwardStartRequestStatus(net::URLRequest* request) override;
  void ForwardDoneRequestStatus(net::URLRequest* request) override;
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  int OnBeforeStartTransaction(net::URLRequest* request,
                               net::CompletionOnceCallback callback,
                               net::HttpRequestHeaders* headers) override;
  void OnStartTransaction(net::URLRequest* request,
                          const net::HttpRequestHeaders& headers) override;
  int OnHeadersReceived(
      net::URLRequest* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override;
  void OnURLRequestDestroyed(net::URLRequest* request) override;
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      AuthCallback callback,
      net::AuthCredentials* credentials) override;

  extensions::WebRequestInfo* GetWebRequestInfo(net::URLRequest* request) {
    auto it = active_requests_.find(request);
    if (it == active_requests_.end()) {
      it = active_requests_
               .emplace(request,
                        std::make_unique<extensions::WebRequestInfo>(request))
               .first;
    }
    return it->second.get();
  }

  std::map<net::URLRequest*, std::unique_ptr<extensions::WebRequestInfo>>
      active_requests_;
  scoped_refptr<extensions::EventRouterForwarder> event_router_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsNetworkDelegateImpl);
};

ChromeExtensionsNetworkDelegateImpl::ChromeExtensionsNetworkDelegateImpl(
    extensions::EventRouterForwarder* event_router) {
  DCHECK(event_router);
  event_router_ = event_router;
}

ChromeExtensionsNetworkDelegateImpl::~ChromeExtensionsNetworkDelegateImpl() {
  DCHECK(active_requests_.empty());
}


void ChromeExtensionsNetworkDelegateImpl::ForwardStartRequestStatus(
    net::URLRequest* request) {
  ForwardRequestStatus(REQUEST_STARTED, request, profile_);
}

void ChromeExtensionsNetworkDelegateImpl::ForwardDoneRequestStatus(
    net::URLRequest* request) {
  ForwardRequestStatus(REQUEST_DONE, request, profile_);
}

int ChromeExtensionsNetworkDelegateImpl::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  // NOTE: A redirected URLRequest results in another invocation of
  // OnBeforeURLRequest for the same URLRequest object but in a different state.
  // Therefore we always replace the mapped WebRequestInfo for that URLRequest
  // with a newly constructed one here.
  std::unique_ptr<extensions::WebRequestInfo>* web_request_info =
      &active_requests_[request];
  *web_request_info = std::make_unique<extensions::WebRequestInfo>(request);

  bool should_collapse_initiator = false;
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      profile_, extension_info_map_.get(), web_request_info->get(),
      std::move(callback), new_url, &should_collapse_initiator);
  if (should_collapse_initiator) {
    auto* info = ResourceRequestInfo::ForRequest(request);
    DCHECK(info);
    info->SetResourceRequestBlockedReason(
        blink::ResourceRequestBlockedReason::kCollapsedByClient);
  }

  return result;
}

int ChromeExtensionsNetworkDelegateImpl::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      profile_, extension_info_map_.get(), GetWebRequestInfo(request),
      std::move(callback), headers);
}

void ChromeExtensionsNetworkDelegateImpl::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      profile_, extension_info_map_.get(), GetWebRequestInfo(request), headers);
}

int ChromeExtensionsNetworkDelegateImpl::OnHeadersReceived(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      profile_, extension_info_map_.get(), GetWebRequestInfo(request),
      std::move(callback), original_response_headers, override_response_headers,
      allowed_unsafe_redirect_url);
}

void ChromeExtensionsNetworkDelegateImpl::OnBeforeRedirect(
    net::URLRequest* request,
    const GURL& new_location) {
  auto* info = GetWebRequestInfo(request);
  info->AddResponseInfoFromURLRequest(request);
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      profile_, extension_info_map_.get(), info, new_location);
}

void ChromeExtensionsNetworkDelegateImpl::OnResponseStarted(
    net::URLRequest* request,
    int net_error) {
  auto* info = GetWebRequestInfo(request);
  info->AddResponseInfoFromURLRequest(request);
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      profile_, extension_info_map_.get(), info, net_error);
}

void ChromeExtensionsNetworkDelegateImpl::OnCompleted(net::URLRequest* request,
                                                      bool started,
                                                      int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        profile_, extension_info_map_.get(), GetWebRequestInfo(request),
        started, net_error);
    return;
  }

  bool is_redirect = request->response_headers() &&
                     net::HttpResponseHeaders::IsRedirectResponseCode(
                         request->response_headers()->response_code());
  if (!is_redirect) {
    ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
        profile_, extension_info_map_.get(), GetWebRequestInfo(request),
        net_error);
  }
}

void ChromeExtensionsNetworkDelegateImpl::OnURLRequestDestroyed(
    net::URLRequest* request) {
  auto it = active_requests_.find(request);
  ExtensionWebRequestEventRouter::GetInstance()->OnRequestWillBeDestroyed(
      profile_, it->second.get());
  active_requests_.erase(it);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeExtensionsNetworkDelegateImpl::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    AuthCallback callback,
    net::AuthCredentials* credentials) {
  auto* info = GetWebRequestInfo(request);
  info->AddResponseInfoFromURLRequest(request);
  return ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      profile_, extension_info_map_.get(), info, auth_info, std::move(callback),
      credentials);
}

}  // namespace

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
ChromeExtensionsNetworkDelegate* ChromeExtensionsNetworkDelegate::Create(
    extensions::EventRouterForwarder* event_router) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return new ChromeExtensionsNetworkDelegateImpl(event_router);
#else
  return new ChromeExtensionsNetworkDelegate();
#endif
}

ChromeExtensionsNetworkDelegate::ChromeExtensionsNetworkDelegate()
    : profile_(NULL) {
}

ChromeExtensionsNetworkDelegate::~ChromeExtensionsNetworkDelegate() {}

void ChromeExtensionsNetworkDelegate::set_extension_info_map(
    extensions::InfoMap* extension_info_map) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_info_map_ = extension_info_map;
#endif
}

void ChromeExtensionsNetworkDelegate::ForwardStartRequestStatus(
    net::URLRequest* request) {
}

void ChromeExtensionsNetworkDelegate::ForwardDoneRequestStatus(
    net::URLRequest* request) {
}

// Notifies the extensions::ProcessManager that a request has started or stopped

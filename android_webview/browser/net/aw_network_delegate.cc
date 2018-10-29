// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_network_delegate.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/net/aw_web_resource_request.h"
#include "base/android/build_info.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace android_webview {

namespace {

void OnReceivedHttpErrorOnUiThread(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const AwWebResourceRequest& request,
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> http_error_info) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContentsGetter(web_contents_getter);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedHttpError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedHttpError(request, std::move(http_error_info));
}

}  // namespace

AwNetworkDelegate::AwNetworkDelegate() {}

AwNetworkDelegate::~AwNetworkDelegate() {
}

int AwNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  DCHECK(headers);
  headers->SetHeaderIfMissing(
      "X-Requested-With",
      base::android::BuildInfo::GetInstance()->host_package_name());
  return net::OK;
}

void AwNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {}

int AwNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (original_response_headers->response_code() >= 400) {
    const content::ResourceRequestInfo* request_info =
        content::ResourceRequestInfo::ForRequest(request);
    // A request info may not exist for requests not originating from content.
    if (request_info == nullptr)
        return net::OK;

    // extract out the info we need before posting to UI thread.
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> error_info =
        AwContentsClientBridge::ExtractHttpErrorInfo(original_response_headers);

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&OnReceivedHttpErrorOnUiThread,
                       request_info->GetWebContentsGetterForRequest(),
                       AwWebResourceRequest(*request), std::move(error_info)));
  }
  return net::OK;
}

bool AwNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                        const net::CookieList& cookie_list,
                                        bool allow_from_caller) {
  return allow_from_caller &&
         AwCookieAccessPolicy::GetInstance()->OnCanGetCookies(request,
                                                              cookie_list);
}

bool AwNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                       const net::CanonicalCookie& cookie,
                                       net::CookieOptions* options,
                                       bool allow_from_caller) {
  return allow_from_caller &&
         AwCookieAccessPolicy::GetInstance()->OnCanSetCookie(request, cookie,
                                                             options);
}

bool AwNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}

}  // namespace android_webview

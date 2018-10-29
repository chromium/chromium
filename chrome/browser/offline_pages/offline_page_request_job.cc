// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_job.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request.h"

namespace offline_pages {

namespace {

const char kUserDataKey[] = "offline_page_key";

// Contains the info to handle offline page request.
class OfflinePageRequestInfo : public base::SupportsUserData::Data {
 public:
  OfflinePageRequestInfo() : use_default_(false) {}
  ~OfflinePageRequestInfo() override {}

  static OfflinePageRequestInfo* GetFromRequest(
      const net::URLRequest* request) {
    return static_cast<OfflinePageRequestInfo*>(
        request->GetUserData(&kUserDataKey));
  }

  bool use_default() const { return use_default_; }
  void set_use_default(bool use_default) { use_default_ = use_default; }

 private:
  // True if the next time this request is started, the request should be
  // serviced from the default handler.
  bool use_default_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestInfo);
};

}  // namespace

// static
OfflinePageRequestJob* OfflinePageRequestJob::Create(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  const content::ResourceRequestInfo* resource_request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!resource_request_info)
    return nullptr;

  // Ignore the requests not for the main resource.
  if (resource_request_info->GetResourceType() !=
      content::RESOURCE_TYPE_MAIN_FRAME) {
    return nullptr;
  }

  // Ignore non-http/https requests.
  if (!request->url().SchemeIsHTTPOrHTTPS())
    return nullptr;

  // Ignore requests other than GET.
  if (request->method() != "GET")
    return nullptr;

  OfflinePageRequestInfo* info =
      OfflinePageRequestInfo::GetFromRequest(request);
  if (info) {
    // Fall back to default which is set when an offline page cannot be served,
    // either page not found or online version desired.
    if (info->use_default())
      return nullptr;
  } else {
    request->SetUserData(&kUserDataKey,
                         std::make_unique<OfflinePageRequestInfo>());
  }

  return new OfflinePageRequestJob(request, network_delegate);
}

OfflinePageRequestJob::OfflinePageRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate) {}

OfflinePageRequestJob::~OfflinePageRequestJob() {}

void OfflinePageRequestJob::SetWebContentsGetterForTesting(
    OfflinePageRequestHandler::Delegate::WebContentsGetter
        web_contents_getter) {
  web_contents_getter_ = web_contents_getter;
}

void OfflinePageRequestJob::SetTabIdGetterForTesting(
    OfflinePageRequestHandler::Delegate::TabIdGetter tab_id_getter) {
  tab_id_getter_ = tab_id_getter;
}

void OfflinePageRequestJob::Start() {
  request_handler_ = std::make_unique<OfflinePageRequestHandler>(
      request()->url(), request()->extra_request_headers(), this);
  request_handler_->Start();
}

void OfflinePageRequestJob::Kill() {
  request_handler_->Kill();
  net::URLRequestJob::Kill();
}

int OfflinePageRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size) {
  return request_handler_->ReadRawData(dest, dest_size);
}

void OfflinePageRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  scoped_refptr<net::HttpResponseHeaders> redirect_headers =
      request_handler_->GetRedirectHeaders();
  if (!redirect_headers) {
    net::URLRequestJob::GetResponseInfo(info);
    return;
  }

  info->headers = redirect_headers;
  info->request_time = base::Time::Now();
  info->response_time = info->request_time;
}

void OfflinePageRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  // Set send_start and send_end to receive_redirect_headers_end_ to be
  // consistent with network cache behavior.
  load_timing_info->send_start = base::TimeTicks::Now();
  load_timing_info->send_end = load_timing_info->send_start;
  load_timing_info->receive_headers_end = load_timing_info->send_start;
}

bool OfflinePageRequestJob::CopyFragmentOnRedirect(const GURL& location) const {
  return false;
}

bool OfflinePageRequestJob::GetMimeType(std::string* mime_type) const {
  if (request_handler_->IsServingOfflinePage() &&
      request()->status().is_success()) {
    *mime_type = "multipart/related";
    return true;
  }
  return false;
}

void OfflinePageRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
}

void OfflinePageRequestJob::FallbackToDefault() {
  OfflinePageRequestInfo* info =
      OfflinePageRequestInfo::GetFromRequest(request());
  DCHECK(info);
  info->set_use_default(true);

  net::URLRequestJob::NotifyRestartRequired();
}

void OfflinePageRequestJob::NotifyStartError(int error) {
  net::URLRequestJob::NotifyStartError(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, error));
}

void OfflinePageRequestJob::NotifyHeadersComplete(int64_t file_size) {
  set_expected_content_size(file_size);
  net::URLRequestJob::NotifyHeadersComplete();
}

void OfflinePageRequestJob::NotifyReadRawDataComplete(int bytes_read) {
  net::URLRequestJob::ReadRawDataComplete(bytes_read);
}

void OfflinePageRequestJob::SetOfflinePageNavigationUIData(
    bool is_offline_page) {
  // This method should be called before the response data is received.
  DCHECK(!has_response_started());

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());
  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(info->GetNavigationUIData());
  if (navigation_data) {
    std::unique_ptr<OfflinePageNavigationUIData> offline_page_data =
        std::make_unique<OfflinePageNavigationUIData>(is_offline_page);
    navigation_data->SetOfflinePageNavigationUIData(
        std::move(offline_page_data));
  }
}

bool OfflinePageRequestJob::ShouldAllowPreview() const {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());

  bool preview_allowed =
      info && (info->GetPreviewsState() & content::OFFLINE_PAGE_ON);
  return preview_allowed;
}

int OfflinePageRequestJob::GetPageTransition() const {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());
  return info ? static_cast<int>(info->GetPageTransition()) : 0;
}

OfflinePageRequestHandler::Delegate::WebContentsGetter
OfflinePageRequestJob::GetWebContentsGetter() const {
  if (!web_contents_getter_.is_null())
    return web_contents_getter_;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());
  return info ? info->GetWebContentsGetterForRequest()
              : OfflinePageRequestHandler::Delegate::WebContentsGetter();
}

OfflinePageRequestHandler::Delegate::TabIdGetter
OfflinePageRequestJob::GetTabIdGetter() const {
  if (!tab_id_getter_.is_null())
    return tab_id_getter_;
  return base::Bind(&OfflinePageUtils::GetTabId);
}

}  // namespace offline_pages

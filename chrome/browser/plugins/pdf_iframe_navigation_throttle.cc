// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include <string>

#include "base/feature_list.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pdf_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "content/public/browser/plugin_service.h"
#endif

PDFIFrameNavigationThrottle::PDFIFrameNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PDFIFrameNavigationThrottle::~PDFIFrameNavigationThrottle() {}

const char* PDFIFrameNavigationThrottle::GetNameForLogging() {
  return "PDFIFrameNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (handle->IsInMainFrame())
    return nullptr;

#if BUILDFLAG(ENABLE_PLUGINS)
  content::WebPluginInfo pdf_plugin_info;
  static const base::FilePath pdf_plugin_path(
      ChromeContentClient::kPDFPluginPath);
  content::PluginService::GetInstance()->GetPluginInfoByPath(pdf_plugin_path,
                                                             &pdf_plugin_info);

  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  int process_id =
      handle->GetWebContents()->GetMainFrame()->GetProcess()->GetID();
  int routing_id = handle->GetWebContents()->GetMainFrame()->GetRoutingID();
  content::ResourceContext* resource_context =
      handle->GetWebContents()->GetBrowserContext()->GetResourceContext();
  if (filter->IsPluginAvailable(process_id, routing_id, resource_context,
                                handle->GetURL(), url::Origin(),
                                &pdf_plugin_info)) {
    return nullptr;
  }
#endif

  // If ENABLE_PLUGINS is false, the PDF plugin is not available, so we should
  // always intercept PDF iframe navigations.
  return std::make_unique<PDFIFrameNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
PDFIFrameNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  if (!response_headers)
    return content::NavigationThrottle::PROCEED;

  std::string mime_type;
  response_headers->GetMimeType(&mime_type);
  if (mime_type != kPDFMimeType)
    return content::NavigationThrottle::PROCEED;

  // We MUST download responses marked as attachments rather than showing
  // a placeholder.
  if (content::download_utils::MustDownload(navigation_handle()->GetURL(),
                                            response_headers, mime_type)) {
    return content::NavigationThrottle::PROCEED;
  }

  ReportPDFLoadStatus(PDFLoadStatus::kLoadedIframePdfWithNoPdfViewer);

  if (!base::FeatureList::IsEnabled(features::kClickToOpenPDFPlaceholder))
    return content::NavigationThrottle::PROCEED;

  std::string html = GetPDFPlaceholderHTML(navigation_handle()->GetURL());
  GURL data_url("data:text/html," + net::EscapePath(html));

  navigation_handle()->GetWebContents()->OpenURL(
      content::OpenURLParams(data_url, navigation_handle()->GetReferrer(),
                             navigation_handle()->GetFrameTreeNodeId(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_AUTO_SUBFRAME, false));

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

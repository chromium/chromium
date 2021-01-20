// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include <string>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pdf_util.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "content/public/browser/plugin_service.h"
#endif

namespace {

// Used to scope the posted navigation task to the lifetime of |web_contents|.
class PdfWebContentsLifetimeHelper
    : public content::WebContentsUserData<PdfWebContentsLifetimeHelper> {
 public:
  explicit PdfWebContentsLifetimeHelper(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  base::WeakPtr<PdfWebContentsLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void NavigateIFrameToPlaceholder(const content::OpenURLParams& url_params) {
    web_contents_->OpenURL(url_params);
  }

 private:
  friend class content::WebContentsUserData<PdfWebContentsLifetimeHelper>;

  content::WebContents* const web_contents_;
  base::WeakPtrFactory<PdfWebContentsLifetimeHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PdfWebContentsLifetimeHelper)

#if BUILDFLAG(ENABLE_PLUGINS)
// Returns true if the PDF plugin for |navigation_handle| is enabled. Optionally
// also sets |is_stale| to true if the plugin list needs a reload.
bool IsPDFPluginEnabled(content::NavigationHandle* navigation_handle,
                        bool* is_stale) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  int process_id = web_contents->GetMainFrame()->GetProcess()->GetID();
  int routing_id = web_contents->GetMainFrame()->GetRoutingID();

  content::WebPluginInfo plugin_info;
  return content::PluginService::GetInstance()->GetPluginInfo(
      process_id, routing_id, navigation_handle->GetURL(),
      web_contents->GetMainFrame()->GetLastCommittedOrigin(), kPDFMimeType,
      false /* allow_wildcard */, is_stale, &plugin_info,
      nullptr /* actual_mime_type */);
}
#endif

}  // namespace

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

#if BUILDFLAG(ENABLE_PLUGINS)
  bool is_stale = false;
  bool pdf_plugin_enabled = IsPDFPluginEnabled(navigation_handle(), &is_stale);

  if (is_stale) {
    // On browser start, the plugin list may not be ready yet.
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PDFIFrameNavigationThrottle::OnPluginsLoaded,
                       weak_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }

  // If the plugin was found, proceed on the navigation. Otherwise fall through
  // to the placeholder case.
  if (pdf_plugin_enabled)
    return content::NavigationThrottle::PROCEED;
#endif

  LoadPlaceholderHTML();
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

#if BUILDFLAG(ENABLE_PLUGINS)
void PDFIFrameNavigationThrottle::OnPluginsLoaded(
    const std::vector<content::WebPluginInfo>& plugins) {
  if (IsPDFPluginEnabled(navigation_handle(), nullptr /* is_stale */)) {
    Resume();
  } else {
    LoadPlaceholderHTML();
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
  }
}
#endif

void PDFIFrameNavigationThrottle::LoadPlaceholderHTML() {
  // Prepare the params to navigate to the placeholder.
  std::string html = GetPDFPlaceholderHTML(navigation_handle()->GetURL());
  GURL data_url("data:text/html," + net::EscapePath(html));
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = data_url;
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;

  // Post a task to navigate to the placeholder HTML. We don't navigate
  // synchronously here, as starting a navigation within a navigation is
  // an antipattern. Use a helper object scoped to the WebContents lifetime to
  // scope the navigation task to the WebContents lifetime.
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents)
    return;

  PdfWebContentsLifetimeHelper::CreateForWebContents(web_contents);
  PdfWebContentsLifetimeHelper* helper =
      PdfWebContentsLifetimeHelper::FromWebContents(web_contents);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PdfWebContentsLifetimeHelper::NavigateIFrameToPlaceholder,
                     helper->GetWeakPtr(), std::move(params)));
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_observer_common.h"
#include "chrome/common/buildflags.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

// PluginObserver -------------------------------------------------------------

void PluginObserver::BindPluginHost(
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* plugin_helper = PluginObserver::FromWebContents(web_contents);
  if (!plugin_helper)
    return;
  plugin_helper->plugin_host_receivers_.Bind(rfh, std::move(receiver));
}

PluginObserver::PluginObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PluginObserver>(*web_contents),
      plugin_host_receivers_(web_contents, this) {}

PluginObserver::~PluginObserver() = default;

void PluginObserver::OpenPDF(const GURL& url) {
  content::RenderFrameHost* render_frame_host =
      plugin_host_receivers_.GetCurrentTargetFrame();
  // WebViews should never trigger PDF downloads.
  if (extensions::WebViewGuest::FromRenderFrameHost(render_frame_host))
    return;

  content::Referrer referrer;
  if (!CanOpenPdfUrl(render_frame_host, url,
                     web_contents()->GetLastCommittedURL(), &referrer)) {
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("pdf_plugin_placeholder", R"(
        semantics {
          sender: "PDF Plugin Placeholder"
          description:
            "When the PDF Viewer is unavailable, a placeholder is shown for "
            "embedded PDFs. This placeholder allows the user to download and "
            "open the PDF file via a button."
          trigger:
            "The user clicks the 'View PDF' button in the PDF placeholder."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled via 'Download PDF files instead of "
            "automatically opening them in Chrome' in settings under content. "
            "The feature is disabled by default."
          chrome_policy {
            AlwaysOpenPdfExternally {
              AlwaysOpenPdfExternally: false
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      std::make_unique<download::DownloadUrlParameters>(
          url,
          render_frame_host->GetRenderViewHost()
              ->GetProcess()
              ->GetDeprecatedID(),
          render_frame_host->GetRoutingID(), traffic_annotation);
  params->set_referrer(referrer.url);
  params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));

  web_contents()->GetBrowserContext()->GetDownloadManager()->DownloadUrl(
      std::move(params));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PluginObserver);

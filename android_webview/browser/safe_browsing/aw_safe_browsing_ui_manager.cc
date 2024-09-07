// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/safe_browsing/aw_ping_manager_factory.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"
#include "android_webview/common/aw_paths.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

namespace {

network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  network_context_params->user_agent = GetUserAgent();
  return network_context_params;
}

}  // namespace

AwSafeBrowsingUIManager::AwSafeBrowsingUIManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(timvolodine): verify this is what we want regarding the directory.
  base::FilePath user_data_dir;
  bool result = base::PathService::Get(android_webview::DIR_SAFE_BROWSING,
                                       &user_data_dir);
  DCHECK(result);

  network_context_ =
      std::make_unique<safe_browsing::SafeBrowsingNetworkContext>(
          user_data_dir, /*trigger_migration=*/false,
          base::BindRepeating(CreateDefaultNetworkContextParams));
}

AwSafeBrowsingUIManager::~AwSafeBrowsingUIManager() {}

void AwSafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents =
      safe_browsing::unsafe_resource_util::GetWebContentsForResource(resource);
  // Check the size of the view
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  if (!client || !client->CanShowInterstitial()) {
    OnBlockingPageDone(std::vector<UnsafeResource>{resource}, false,
                       web_contents, resource.url.GetWithEmptyPath(),
                       false /* showed_interstitial */);
    return;
  }
  safe_browsing::BaseUIManager::DisplayBlockingPage(resource);
}

int AwSafeBrowsingUIManager::GetErrorUiType(
    content::WebContents* web_contents) const {
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  return client->GetErrorUiType();
}

void AwSafeBrowsingUIManager::SendThreatDetails(
    content::BrowserContext* browser_context,
    std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest> report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DVLOG(1) << "Sending threat details";
  safe_browsing::AwPingManagerFactory::GetForBrowserContext(browser_context)
      ->ReportThreatDetails(std::move(report));
}

security_interstitials::SecurityInterstitialPage*
AwSafeBrowsingUIManager::CreateBlockingPage(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource,
    bool forward_extension_event,
    std::optional<base::TimeTicks> blocked_page_shown_timestamp) {
  // The AwWebResourceRequest can't be provided yet, since the navigation hasn't
  // started. Once it has, it will be provided via
  // AwSafeBrowsingBlockingPage::CreatedErrorPageNavigation.
  AwSafeBrowsingBlockingPage* blocking_page =
      AwSafeBrowsingBlockingPage::CreateBlockingPage(
          this, contents, blocked_url, unsafe_resource, nullptr,
          blocked_page_shown_timestamp);
  return blocking_page;
}

scoped_refptr<network::SharedURLLoaderFactory>
AwSafeBrowsingUIManager::GetURLLoaderFactory() {
  return network_context_->GetURLLoaderFactory();
}

}  // namespace android_webview

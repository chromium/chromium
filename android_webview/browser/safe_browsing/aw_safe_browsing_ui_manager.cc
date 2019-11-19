// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_blocking_page.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_subresource_helper.h"
#include "android_webview/common/aw_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/ping_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "services/network/public/mojom/network_service.mojom.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

namespace {

std::string GetProtocolConfigClientName() {
  // Return a webview specific client name, see crbug.com/732373 for details.
  return "android_webview";
}

// UMA_HISTOGRAM_* macros expand to a lot of code, so wrap this in a helper.
void RecordIsWebViewViewable(bool isViewable) {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.WebView.Viewable", isViewable);
}

network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
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
          user_data_dir,
          base::BindRepeating(CreateDefaultNetworkContextParams));
}

AwSafeBrowsingUIManager::~AwSafeBrowsingUIManager() {}

void AwSafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = resource.web_contents_getter.Run();
  // Check the size of the view
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  if (!client || !client->CanShowInterstitial()) {
    RecordIsWebViewViewable(false);
    OnBlockingPageDone(std::vector<UnsafeResource>{resource}, false,
                       web_contents, resource.url.GetWithEmptyPath());
    return;
  }
  RecordIsWebViewViewable(true);
  safe_browsing::BaseUIManager::DisplayBlockingPage(resource);
}

void AwSafeBrowsingUIManager::ShowBlockingPageForResource(
    const UnsafeResource& resource) {
  AwSafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

scoped_refptr<network::SharedURLLoaderFactory>
AwSafeBrowsingUIManager::GetURLLoaderFactoryOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!shared_url_loader_factory_on_io_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&AwSafeBrowsingUIManager::CreateURLLoaderFactoryForIO,
                       this,
                       url_loader_factory_on_io_.BindNewPipeAndPassReceiver()));
    shared_url_loader_factory_on_io_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_on_io_.get());
  }
  return shared_url_loader_factory_on_io_;
}

int AwSafeBrowsingUIManager::GetErrorUiType(
    content::WebContents* web_contents) const {
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  return client->GetErrorUiType();
}

void AwSafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ping_manager_) {
    // Lazy creation of ping manager, needs to happen on IO thread.
    ping_manager_ = ::safe_browsing::PingManager::Create(
        network_context_->GetURLLoaderFactory(),
        safe_browsing::GetV4ProtocolConfig(GetProtocolConfigClientName(),
                                           false /* disable_auto_update */));
  }

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details";
    ping_manager_->ReportThreatDetails(serialized);
  }
}

safe_browsing::BaseBlockingPage*
AwSafeBrowsingUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  AwSafeBrowsingSubresourceHelper::CreateForWebContents(contents);
  AwSafeBrowsingBlockingPage* blocking_page =
      AwSafeBrowsingBlockingPage::CreateBlockingPage(
          this, contents, blocked_url, unsafe_resource);
  return blocking_page;
}

void AwSafeBrowsingUIManager::CreateURLLoaderFactoryForIO(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_corb_enabled = false;
  network_context_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(receiver), std::move(url_loader_factory_params));
}

}  // namespace android_webview

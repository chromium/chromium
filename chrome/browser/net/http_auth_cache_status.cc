// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_auth_cache_status.h"

#include "base/check.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_client.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// static
void HttpAuthCacheStatus::CreateForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsUserData<HttpAuthCacheStatus>::CreateForWebContents(
      web_contents);
}

HttpAuthCacheStatus::HttpAuthCacheStatus(content::WebContents* web_contents)
    : content::WebContentsUserData<HttpAuthCacheStatus>(*web_contents),
      content::WebContentsObserver(web_contents) {}

HttpAuthCacheStatus::~HttpAuthCacheStatus() = default;

void HttpAuthCacheStatus::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  if (!resource_load_info.did_use_server_http_auth) {
    return;
  }
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive() ||
      !render_frame_host->IsActive()) {
    return;
  }
  const net::NetworkAnonymizationKey& rfh_nak =
      render_frame_host->GetIsolationInfoForSubresources()
          .network_anonymization_key();

  const net::SchemefulSite subresource_site(resource_load_info.final_url);
  const net::NetworkAnonymizationKey subresource_nak =
      net::NetworkAnonymizationKey::CreateSameSite(subresource_site);

  if (rfh_nak != subresource_nak) {
    // Notify observers that an HTTP Auth request was seen on the current
    // page.
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        render_frame_host,
        blink::mojom::WebFeature::kDidUseServerHttpAuthOnCrossPartitionRequest);
  }
}

// Data key required for WebContentsUserData.
WEB_CONTENTS_USER_DATA_KEY_IMPL(HttpAuthCacheStatus);

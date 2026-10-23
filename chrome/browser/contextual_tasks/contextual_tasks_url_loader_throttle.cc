// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_url_loader_throttle.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/self_deleting.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/google/core/common/google_util.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/isolation_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/extension_features.h"
#endif

namespace contextual_tasks {

namespace {

bool IsEligible(Profile* profile) {
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return false;
#else
  if (!contextual_tasks::IsContextualTasksRearchitectureEnabled()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          extensions_features::kApiContextualTasksPrivate)) {
    return false;
  }

  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  return true;
#endif
}

}  // namespace

// static
std::unique_ptr<ContextualTasksURLLoaderThrottle>
ContextualTasksURLLoaderThrottle::MaybeCreate(
    Profile* profile,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter) {
  if (!IsEligible(profile)) {
    return nullptr;
  }

  return std::make_unique<ContextualTasksURLLoaderThrottle>();
}

ContextualTasksURLLoaderThrottle::ContextualTasksURLLoaderThrottle() = default;

ContextualTasksURLLoaderThrottle::~ContextualTasksURLLoaderThrottle() = default;

void ContextualTasksURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (ShouldAppendHeader(request->url)) {
    request->cors_exempt_headers.SetHeader(
        kContextualTasksSearchCapabilitiesHeaderName,
        GetContextualTasksSearchCapabilitiesVersion());
  }
}

void ContextualTasksURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  if (ShouldAppendHeader(redirect_info->new_url)) {
    headers_update_params->modified_cors_exempt_headers.SetHeader(
        kContextualTasksSearchCapabilitiesHeaderName,
        GetContextualTasksSearchCapabilitiesVersion());
  } else {
    headers_update_params->removed_headers.push_back(
        kContextualTasksSearchCapabilitiesHeaderName);
  }
}

// static
bool ContextualTasksURLLoaderThrottle::ShouldAppendHeader(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme) &&
         google_util::IsGoogleAssociatedDomainUrl(url);
}

ContextualTasksSubresourceProxyingURLLoaderFactory::
    ContextualTasksSubresourceProxyingURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            target_factory_remote,
        base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key) {
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &ContextualTasksSubresourceProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));
}

// static
void ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
    content::BrowserContext* browser_context,
    const net::IsolationInfo& isolation_info,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!browser_context) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!IsEligible(profile)) {
    return;
  }
  if (!isolation_info.top_frame_origin().has_value() ||
      !ContextualTasksURLLoaderThrottle::ShouldAppendHeader(
          isolation_info.top_frame_origin()->GetURL())) {
    return;
  }
  auto [receiver, remote] = factory_builder.Append();
  base::MakeSelfDeleting<ContextualTasksSubresourceProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote));
}

void ContextualTasksSubresourceProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::ResourceRequest modified_request = request;
  if (ContextualTasksURLLoaderThrottle::ShouldAppendHeader(
          modified_request.url)) {
    modified_request.cors_exempt_headers.SetHeader(
        kContextualTasksSearchCapabilitiesHeaderName,
        GetContextualTasksSearchCapabilitiesVersion());
  }

  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        options, modified_request,
                                        std::move(client), traffic_annotation);
}

ContextualTasksSubresourceProxyingURLLoaderFactory::
    ~ContextualTasksSubresourceProxyingURLLoaderFactory() = default;

void ContextualTasksSubresourceProxyingURLLoaderFactory::
    OnTargetFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace contextual_tasks

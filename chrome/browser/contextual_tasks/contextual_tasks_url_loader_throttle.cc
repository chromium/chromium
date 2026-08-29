// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_url_loader_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/google/core/common/google_util.h"
#include "extensions/buildflags/buildflags.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/extension_features.h"
#endif

namespace contextual_tasks {

// static
std::unique_ptr<ContextualTasksURLLoaderThrottle>
ContextualTasksURLLoaderThrottle::MaybeCreate(
    Profile* profile,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter) {
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return nullptr;
#else
  if (!contextual_tasks::IsContextualTasksRearchitectureEnabled()) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(
          extensions_features::kApiContextualTasksPrivate)) {
    return nullptr;
  }

  if (!profile || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return std::make_unique<ContextualTasksURLLoaderThrottle>();
#endif
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

}  // namespace contextual_tasks

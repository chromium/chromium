// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_lacros.h"

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

WebsiteMetricsRetrieverLacros::WebsiteMetricsRetrieverLacros(
    base::WeakPtr<Profile> profile)
    : profile_(profile) {}

WebsiteMetricsRetrieverLacros::~WebsiteMetricsRetrieverLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
}

void WebsiteMetricsRetrieverLacros::GetWebsiteMetrics(
    WebsiteMetricsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  if (!profile_) {
    // Profile destructed, so we return nullptr.
    std::move(callback).Run(nullptr);
    return;
  }
  CHECK(::apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile_.get()))
      << "App service unavailable for profile";
  ::apps::AppServiceProxyLacros* const app_service_proxy =
      ::apps::AppServiceProxyFactory::GetForProfile(profile_.get());
  CHECK(app_service_proxy) << "App service proxy unavailable";
  ::apps::WebsiteMetricsServiceLacros* const website_metrics_service =
      app_service_proxy->WebsiteMetricsService();
  CHECK(website_metrics_service);
  if (website_metrics_service->WebsiteMetrics()) {
    // `WebsiteMetrics` component already initialized, so we return the
    // initialized component.
    std::move(callback).Run(website_metrics_service->WebsiteMetrics());
    return;
  }

  // `WebsiteMetrics` component is not initialized, so we observe initialization
  //  and return initialized component when ready.
  callback_list_.AddUnsafe(std::move(callback));
  if (!init_observer_.IsObservingSource(website_metrics_service)) {
    init_observer_.Observe(website_metrics_service);
  }
}

bool WebsiteMetricsRetrieverLacros::IsObservingSourceForTest(
    ::apps::WebsiteMetricsServiceLacros* website_metrics_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return init_observer_.IsObservingSource(website_metrics_service);
}

void WebsiteMetricsRetrieverLacros::OnWebsiteMetricsInit(
    ::apps::WebsiteMetrics* website_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_list_.Notify(website_metrics);
  init_observer_.Reset();
}

void WebsiteMetricsRetrieverLacros::
    OnWebsiteMetricsServiceLacrosWillBeDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The `WebsiteMetricsServiceLacros` component was destroyed while we were
  // observing initialization of the `WebsiteMetrics` component, so we return
  // nullptr and reset observation.
  callback_list_.Notify(nullptr);
  init_observer_.Reset();
}

}  // namespace reporting

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

AppPlatformMetricsRetriever::AppPlatformMetricsRetriever(
    base::WeakPtr<Profile> profile)
    : profile_(profile) {}

AppPlatformMetricsRetriever::~AppPlatformMetricsRetriever() = default;

void AppPlatformMetricsRetriever::GetAppPlatformMetrics(
    AppPlatformMetricsCallback callback) {
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
  auto* const app_service_proxy =
      ::apps::AppServiceProxyFactory::GetForProfile(profile_.get());
  CHECK(app_service_proxy) << "App service proxy unavailable";
  if (app_service_proxy->AppPlatformMetrics()) {
    // `AppPlatformMetrics` component already initialized, so we return the
    // initialized component.
    std::move(callback).Run(app_service_proxy->AppPlatformMetrics());
    return;
  }

  // `AppPlatformMetrics` component is not initialized, so we observe
  // initialization and return initialized component when ready.
  callback_list_.AddUnsafe(std::move(callback));
  auto* const app_platform_metrics_service =
      app_service_proxy->AppPlatformMetricsService();
  if (!init_observer_.IsObservingSource(app_platform_metrics_service)) {
    init_observer_.Observe(app_platform_metrics_service);
  }
}

bool AppPlatformMetricsRetriever::IsObservingSourceForTest(
    ::apps::AppPlatformMetricsService* app_platform_metrics_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return init_observer_.IsObservingSource(app_platform_metrics_service);
}

void AppPlatformMetricsRetriever::OnAppPlatformMetricsInit(
    ::apps::AppPlatformMetrics* app_platform_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_list_.Notify(app_platform_metrics);
  init_observer_.Reset();
}

void AppPlatformMetricsRetriever::OnAppPlatformMetricsServiceWillBeDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The `AppPlatformMetricsService` component was destroyed while we were
  // observing initialization of the `AppPlatformMetrics` component, so we
  // return nullptr and reset observation.
  callback_list_.Notify(nullptr);
  init_observer_.Reset();
}

}  // namespace reporting

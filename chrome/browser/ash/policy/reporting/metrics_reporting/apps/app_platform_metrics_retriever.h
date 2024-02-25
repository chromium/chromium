// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_PLATFORM_METRICS_RETRIEVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_PLATFORM_METRICS_RETRIEVER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

namespace reporting {

// Retriever implementation that retrieves the `AppPlatformMetrics` component.
// If the component is not initialized, it will observe component initialization
// and return the initialized component when complete.
class AppPlatformMetricsRetriever
    : public ::apps::AppPlatformMetricsService::Observer {
 public:
  using AppPlatformMetricsCallback =
      base::OnceCallback<void(::apps::AppPlatformMetrics*)>;

  explicit AppPlatformMetricsRetriever(base::WeakPtr<Profile> profile);
  AppPlatformMetricsRetriever(const AppPlatformMetricsRetriever&) = delete;
  AppPlatformMetricsRetriever& operator=(const AppPlatformMetricsRetriever&) =
      delete;
  ~AppPlatformMetricsRetriever() override;

  // Retrieves the initialized `AppPlatformMetrics` component and triggers the
  // specified callback when done.
  virtual void GetAppPlatformMetrics(AppPlatformMetricsCallback callback);

  // Returns true if the retriever is observing the specified source for
  // initialization of the `AppPlatformMetrics` component. Only needed for
  // testing purposes.
  bool IsObservingSourceForTest(
      ::apps::AppPlatformMetricsService* app_platform_metrics_service);

 private:
  // AppPlatformMetricsService::Observer
  void OnAppPlatformMetricsInit(
      ::apps::AppPlatformMetrics* app_platform_metrics) override;

  // AppPlatformMetricsService::Observer
  void OnAppPlatformMetricsServiceWillBeDestroyed() override;

  SEQUENCE_CHECKER(sequence_checker_);
  const base::WeakPtr<Profile> profile_;

  // Observer that tracks initialization of the `AppPlatformMetrics` component.
  base::ScopedObservation<::apps::AppPlatformMetricsService,
                          ::apps::AppPlatformMetricsService::Observer>
      init_observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  // List of registered callbacks that need to be triggered once the
  // `AppPlatformMetrics` component is initialized.
  base::OnceCallbackList<void(::apps::AppPlatformMetrics*)> callback_list_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_PLATFORM_METRICS_RETRIEVER_H_

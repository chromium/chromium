// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_ASH_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_ASH_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "chrome/browser/profiles/profile.h"

namespace reporting {

// Retriever implementation that retrieves the `WebsiteMetrics` component in
// Ash. If the component is not initialized, it will observe component
// initialization and return the initialized component when complete.
class WebsiteMetricsRetrieverAsh
    : public WebsiteMetricsRetrieverInterface,
      public ::apps::AppPlatformMetricsService::Observer {
 public:
  explicit WebsiteMetricsRetrieverAsh(base::WeakPtr<Profile> profile);
  WebsiteMetricsRetrieverAsh(const WebsiteMetricsRetrieverAsh&) = delete;
  WebsiteMetricsRetrieverAsh& operator=(const WebsiteMetricsRetrieverAsh&) =
      delete;
  ~WebsiteMetricsRetrieverAsh() override;

  // WebsiteMetricsRetrieverInterface:
  void GetWebsiteMetrics(WebsiteMetricsCallback callback) override;

  // Returns true if the retriever is observing the specified source for
  // initialization of the `WebsiteMetrics` component. Only needed for testing
  // purposes.
  bool IsObservingSourceForTest(
      ::apps::AppPlatformMetricsService* app_platform_metrics_service);

 private:
  // AppPlatformMetricsService::Observer:
  void OnWebsiteMetricsInit(::apps::WebsiteMetrics* website_metrics) override;

  // AppPlatformMetricsService::Observer:
  void OnAppPlatformMetricsServiceWillBeDestroyed() override;

  SEQUENCE_CHECKER(sequence_checker_);

  const base::WeakPtr<Profile> profile_;

  // Observer that tracks initialization of the `WebsiteMetrics` component.
  base::ScopedObservation<::apps::AppPlatformMetricsService,
                          ::apps::AppPlatformMetricsService::Observer>
      init_observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  // List of registered callbacks that need to be triggered once the
  // `WebsiteMetrics` component is initialized.
  base::OnceCallbackList<void(::apps::WebsiteMetrics*)> callback_list_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_ASH_H_

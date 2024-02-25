// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_LACROS_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "chrome/browser/profiles/profile.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace reporting {

// Retriever implementation that retrieves the `WebsiteMetrics` component in
// Lacros. If the component is not initialized, it will observe component
// initialization and return the initialized component when complete.
class WebsiteMetricsRetrieverLacros
    : public WebsiteMetricsRetrieverInterface,
      public ::apps::WebsiteMetricsServiceLacros::Observer {
 public:
  explicit WebsiteMetricsRetrieverLacros(base::WeakPtr<Profile> profile);
  WebsiteMetricsRetrieverLacros(const WebsiteMetricsRetrieverLacros&) = delete;
  WebsiteMetricsRetrieverLacros& operator=(
      const WebsiteMetricsRetrieverLacros&) = delete;
  ~WebsiteMetricsRetrieverLacros() override;

  // WebsiteMetricsRetrieverInterface:
  void GetWebsiteMetrics(WebsiteMetricsCallback callback) override;

  // Returns true if the retriever is observing the specified source for
  // initialization of the `WebsiteMetrics` component. False otherwise. Only
  // needed for testing purposes.
  bool IsObservingSourceForTest(
      ::apps::WebsiteMetricsServiceLacros* website_metrics_service);

 private:
  // WebsiteMetricsServiceLacros::Observer:
  void OnWebsiteMetricsInit(::apps::WebsiteMetrics* website_metrics) override;

  // WebsiteMetricsServiceLacros::Observer:
  void OnWebsiteMetricsServiceLacrosWillBeDestroyed() override;

  SEQUENCE_CHECKER(sequence_checker_);

  const base::WeakPtr<Profile> profile_;

  // Observer that tracks initialization of the `WebsiteMetrics` component.
  base::ScopedObservation<::apps::WebsiteMetricsServiceLacros,
                          ::apps::WebsiteMetricsServiceLacros::Observer>
      init_observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  // List of registered callbacks that need to be triggered once the
  // `WebsiteMetrics` component is initialized.
  base::OnceCallbackList<void(::apps::WebsiteMetrics*)> callback_list_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_LACROS_H_

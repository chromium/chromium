// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_INTERFACE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

namespace reporting {

// Retriever interface for components that retrieve the initialized
// `WebsiteMetrics` component.
class WebsiteMetricsRetrieverInterface {
 public:
  using WebsiteMetricsCallback =
      base::OnceCallback<void(::apps::WebsiteMetrics*)>;

  WebsiteMetricsRetrieverInterface(const WebsiteMetricsRetrieverInterface&) =
      delete;
  WebsiteMetricsRetrieverInterface& operator=(
      const WebsiteMetricsRetrieverInterface&) = delete;
  virtual ~WebsiteMetricsRetrieverInterface() = default;

  // Retrieves the initialized `WebsiteMetrics` component and triggers the
  // specified callback.
  virtual void GetWebsiteMetrics(WebsiteMetricsCallback callback) = 0;

 protected:
  WebsiteMetricsRetrieverInterface() = default;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_METRICS_RETRIEVER_INTERFACE_H_

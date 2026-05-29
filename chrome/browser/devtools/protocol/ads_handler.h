// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_ADS_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_ADS_HANDLER_H_

#include "chrome/browser/devtools/protocol/ads.h"
#include "content/public/browser/web_contents_observer.h"

namespace page_load_metrics {
class AdsPageLoadMetricsObserver;
}

// Implements the "Ads" DevTools protocol domain.
//
// This class bridges the DevTools frontend with the backend
// `AdsPageLoadMetricsObserver`. It handles requests from the frontend to
// retrieve page-level ad statistics.
class AdsHandler : public protocol::Ads::Backend,
                   public content::WebContentsObserver {
 public:
  AdsHandler(content::WebContents* web_contents,
             protocol::UberDispatcher* dispatcher,
             bool is_trusted);
  ~AdsHandler() override;
  AdsHandler(const AdsHandler&) = delete;
  AdsHandler& operator=(const AdsHandler&) = delete;

 private:
  // protocol::Ads::Backend:
  protocol::Response GetAdMetrics(
      std::unique_ptr<protocol::Ads::AdMetrics>* out_metrics) override;

  page_load_metrics::AdsPageLoadMetricsObserver*
  GetAdsPageLoadMetricsObserver();
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_ADS_HANDLER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICES_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICES_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace metrics {

class MetricsServicesWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsServicesWebContentsObserver> {
 public:
  ~MetricsServicesWebContentsObserver() override;

 private:
  explicit MetricsServicesWebContentsObserver(
      content::WebContents* web_contents);
  MetricsServicesWebContentsObserver(
      const MetricsServicesWebContentsObserver&) = delete;
  MetricsServicesWebContentsObserver& operator=(
      const MetricsServicesWebContentsObserver&) = delete;
  friend class content::WebContentsUserData<MetricsServicesWebContentsObserver>;

  // content::WebContentsObserver overrides:
  void DidStartLoading() override;
  void DidStopLoading() override;
  void OnRendererUnresponsive(content::RenderProcessHost* host) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICES_WEB_CONTENTS_OBSERVER_H_

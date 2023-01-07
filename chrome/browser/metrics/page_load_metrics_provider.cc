// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/page_load_metrics_provider.h"

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

PageLoadMetricsProvider::PageLoadMetricsProvider() {}

PageLoadMetricsProvider::~PageLoadMetricsProvider() {}

void PageLoadMetricsProvider::OnAppEnterBackground() {
  for (const TabModel* model : TabModelList::models()) {
    for (int tab_index = 0; tab_index < model->GetTabCount(); ++tab_index) {
      content::WebContents* web_contents = model->GetWebContentsAt(tab_index);
      if (!web_contents)
        continue;
      page_load_metrics::MetricsWebContentsObserver* observer =
          page_load_metrics::MetricsWebContentsObserver::FromWebContents(
              web_contents);
      if (observer)
        observer->FlushMetricsOnAppEnterBackground();
    }
  }
}

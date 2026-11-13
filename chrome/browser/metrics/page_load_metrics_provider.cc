// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/page_load_metrics_provider.h"

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/tabs/public/tab_interface.h"

PageLoadMetricsProvider::PageLoadMetricsProvider() = default;

PageLoadMetricsProvider::~PageLoadMetricsProvider() = default;

void PageLoadMetricsProvider::OnAppEnterBackground() {
  TabModelList::ForEachAll([](TabModel* model) {
    for (tabs::TabInterface* tab : model->GetAllTabs()) {
      content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
      if (!web_contents) {
        continue;
      }
      page_load_metrics::MetricsWebContentsObserver* observer =
          page_load_metrics::MetricsWebContentsObserver::FromWebContents(
              web_contents);
      if (observer) {
        observer->FlushMetricsOnAppEnterBackground();
      }
    }
    return true;
  });
}

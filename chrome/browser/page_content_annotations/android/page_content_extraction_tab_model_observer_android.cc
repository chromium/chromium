// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/android/page_content_extraction_tab_model_observer_android.h"

#include <set>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace {
constexpr base::TimeDelta kMetricsComputationDelay = base::Minutes(1);
}  // namespace

namespace page_content_annotations {

PageContentExtractionTabModelObserverAndroid::
    PageContentExtractionTabModelObserverAndroid(
        Profile* profile,
        PageContentExtractionService* service)
    : profile_(profile), service_(service) {
  TabModelList::AddObserver(this);
  for (TabModel* tab_model : TabModelList::models()) {
    OnTabModelAdded(tab_model);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageContentExtractionTabModelObserverAndroid::
                         RunStartupMetricsComputation,
                     weak_ptr_factory_.GetWeakPtr()),
      kMetricsComputationDelay);
}

PageContentExtractionTabModelObserverAndroid::
    ~PageContentExtractionTabModelObserverAndroid() {
  TabModelList::RemoveObserver(this);
}

void PageContentExtractionTabModelObserverAndroid::OnTabModelAdded(
    TabModel* tab_model) {
  if (tab_model->GetProfile() != profile_) {
    return;
  }
  tab_model_observations_.AddObservation(tab_model);
}

void PageContentExtractionTabModelObserverAndroid::OnTabModelRemoved(
    TabModel* tab_model) {
  if (tab_model->GetProfile() != profile_) {
    return;
  }
  tab_model_observations_.RemoveObservation(tab_model);
}

void PageContentExtractionTabModelObserverAndroid::OnFinishingTabClosure(
    TabAndroid* tab,
    TabModel::TabClosingSource source) {
  service_->OnTabClosed(tab->GetAndroidId());
}

void PageContentExtractionTabModelObserverAndroid::
    RunStartupMetricsComputation() {
  std::set<int64_t> active_tab_ids;
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_) {
      continue;
    }
    for (int i = 0; i < tab_model->GetTabCount(); ++i) {
      TabAndroid* tab = tab_model->GetTabAt(i);
      const GURL& url = tab->GetURL();
      // This should ideally run eligibility check for APC. But, this is
      // approximate and quick to get an idea of the overall cache stats.
      if (base::Time::Now() - tab->GetLastShownTimestamp() >
              base::Days(features::kPageContentCacheMaxCacheAgeInDays.Get()) ||
          !url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
        continue;
      }

      active_tab_ids.insert(tab->GetAndroidId());
    }
  }
  if (service_->GetPageContentCache()) {
    service_->GetPageContentCache()->RecordMetrics(std::move(active_tab_ids));
  }
}

}  // namespace page_content_annotations

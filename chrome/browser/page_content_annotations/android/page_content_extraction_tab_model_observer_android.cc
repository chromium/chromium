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

void PageContentExtractionTabModelObserverAndroid::WillCloseTab(
    TabAndroid* tab) {
  // Observe WillCloseTab instead of OnFinishingTabClosure since we might never
  // receive this observation if the app quit before the undo timeout. It is
  // more reliable to observe this and undo closure.
  service_->OnTabClosed(tab->GetAndroidId());
}

void PageContentExtractionTabModelObserverAndroid::TabClosureUndone(
    TabAndroid* tab) {
  service_->OnTabCloseUndone(tab->GetAndroidId());
}

void PageContentExtractionTabModelObserverAndroid::OnTabStateInitialized() {
  std::set<int64_t> active_tab_ids;
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_) {
      continue;
    }
    for (int i = 0; i < tab_model->GetTabCount(); ++i) {
      TabAndroid* tab = tab_model->GetTabAt(i);
      const GURL& url = tab->GetURL();
      // This computes a superset of eligible tabs since it does not check APC
      // eligibility for the tabs. It is ok to include some ineligible tabs
      // since it is used to clean up non-existant (stale) tabs from cache to
      // reduce disk space.
      if (base::Time::Now() - tab->GetLastShownTimestamp() >
              base::Days(features::kPageContentCacheMaxCacheAgeInDays.Get()) ||
          !url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
        continue;
      }

      active_tab_ids.insert(tab->GetAndroidId());
    }
  }
  if (service_->GetPageContentCache()) {
    service_->RunCleanUpTasksWithActiveTabs(std::move(active_tab_ids));
  }
}

}  // namespace page_content_annotations

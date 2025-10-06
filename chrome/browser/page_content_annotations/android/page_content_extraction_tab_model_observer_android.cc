// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/android/page_content_extraction_tab_model_observer_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/page_content_annotations/page_content_cache_handler.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/profiles/profile.h"
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

void PageContentExtractionTabModelObserverAndroid::OnFinishingTabClosure(
    TabAndroid* tab,
    TabModel::TabClosingSource source) {
  service_->OnTabClosed(tab->GetAndroidId());
}

}  // namespace page_content_annotations

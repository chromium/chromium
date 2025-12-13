// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_ANDROID_PAGE_CONTENT_EXTRACTION_TAB_MODEL_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_ANDROID_PAGE_CONTENT_EXTRACTION_TAB_MODEL_OBSERVER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/tabwindow/tab_window_manager_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

class Profile;
class TabAndroid;

namespace page_content_annotations {

class PageContentExtractionService;

// Observes TabModelList and TabModel to provide tab-related notifications
// for page content annotations. Obervser is one per profile, and ignores any
// observations from incognito and other profiles.
class PageContentExtractionTabModelObserverAndroid
    : public TabModelListObserver,
      public TabModelObserver,
      public tab_window::TabWindowManagerObserver,
      public base::SupportsUserData::Data {
 public:
  explicit PageContentExtractionTabModelObserverAndroid(
      Profile* profile,
      PageContentExtractionService* service);
  ~PageContentExtractionTabModelObserverAndroid() override;

  PageContentExtractionTabModelObserverAndroid(
      const PageContentExtractionTabModelObserverAndroid&) = delete;
  PageContentExtractionTabModelObserverAndroid& operator=(
      const PageContentExtractionTabModelObserverAndroid&) = delete;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

  // TabModelObserver:
  void WillCloseTab(TabAndroid* tab) override;
  void TabClosureUndone(TabAndroid* tab) override;

  // tab_window::TabWindowManagerObserver:
  void OnTabStateInitialized() override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<PageContentExtractionService> service_;

  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};

  base::WeakPtrFactory<PageContentExtractionTabModelObserverAndroid>
      weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_ANDROID_PAGE_CONTENT_EXTRACTION_TAB_MODEL_OBSERVER_ANDROID_H_

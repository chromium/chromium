// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_TAB_STRIP_MODEL_OBSERVER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserWindowInterface;
class Profile;
class ProfileBrowserCollection;
class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {

class PageContentExtractionService;

// Observes `ProfileBrowserCollection` and `TabStripModel` to provide
// tab-related notifications for page content annotations on desktop platforms.
class PageContentExtractionTabStripModelObserver final
    : public BrowserCollectionObserver,
      public TabStripModelObserver,
      public base::SupportsUserData::Data {
 public:
  // `profile` and `service` must be non-null and must outlive `this`.
  PageContentExtractionTabStripModelObserver(
      Profile* profile,
      PageContentExtractionService* service);
  ~PageContentExtractionTabStripModelObserver() override;

  PageContentExtractionTabStripModelObserver(
      const PageContentExtractionTabStripModelObserver&) = delete;
  PageContentExtractionTabStripModelObserver& operator=(
      const PageContentExtractionTabStripModelObserver&) = delete;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  friend class PageContentExtractionTabStripModelObserverTest;

  void EvictFromCache(content::WebContents* contents);
  bool ShouldPreserveCacheOnTabClose(
      const TabStripModel* tab_strip_model) const;
  void OnSessionRestored(Profile* profile, int num_tabs_restored);
  void RunCleanUpTasksIfNotRestoring();
  void RunCleanUpTasks();

  const raw_ref<Profile> profile_;
  const raw_ref<PageContentExtractionService> service_;

  // Startup cache cleanup runs at most once per profile session.
  bool has_run_cleanup_tasks_ = false;

  base::CallbackListSubscription session_restore_subscription_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  base::WeakPtrFactory<PageContentExtractionTabStripModelObserver>
      weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_TAB_STRIP_MODEL_OBSERVER_H_

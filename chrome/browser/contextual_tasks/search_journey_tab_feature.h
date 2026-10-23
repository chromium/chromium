// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_JOURNEY_TAB_FEATURE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_JOURNEY_TAB_FEATURE_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;
class TemplateURLService;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

class SearchJourneyTracker;

// Per-tab observer that forwards clipboard copy events, tab activation changes,
// committed default search provider navigations, and tab destruction/discard
// events to the profile's `SearchJourneyTracker`.
class SearchJourneyTabFeature : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(SearchJourneyTabFeature);

  explicit SearchJourneyTabFeature(tabs::TabInterface& tab);
  SearchJourneyTabFeature(const SearchJourneyTabFeature&) = delete;
  SearchJourneyTabFeature& operator=(const SearchJourneyTabFeature&) = delete;
  ~SearchJourneyTabFeature() override;

  static SearchJourneyTabFeature* From(tabs::TabInterface* tab);

  // Returns whether `RestoreJourneyInSidePanel()` would open the side panel
  // now.
  bool CanRestoreJourneyInSidePanel() const;

  // Opens the Contextual Tasks side panel next to this tab, restoring the AI
  // Overview of the journey whose Tab A is this tab as the first turn of an AI
  // Mode thread. Returns whether the side panel was opened.
  bool RestoreJourneyInSidePanel();

  // content::WebContentsObserver:
  void OnTextCopiedToClipboard(content::RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void OnTabDidActivate(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);
  void NotifyTabDestroyed(content::WebContents* contents);

  // Returns the AI Mode URL that restores this tab's journey in the side panel,
  // or an empty URL if it can't be restored now.
  GURL GetRestoreUrl() const;

  const raw_ref<tabs::TabInterface> tab_;
  raw_ptr<SearchJourneyTracker> tracker_ = nullptr;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  base::CallbackListSubscription will_discard_contents_subscription_;
  base::CallbackListSubscription did_activate_subscription_;
  base::CallbackListSubscription will_deactivate_subscription_;
  ui::ScopedUnownedUserData<SearchJourneyTabFeature> scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SEARCH_JOURNEY_TAB_FEATURE_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TAB_FEATURE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TAB_FEATURE_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

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

class CopySearchJourneyTracker;

// Per-tab observer that forwards clipboard copy events, committed default
// search provider navigations, and tab destruction/discard events to the
// profile's `CopySearchJourneyTracker`.
class CopySearchJourneyTabFeature : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(CopySearchJourneyTabFeature);

  explicit CopySearchJourneyTabFeature(tabs::TabInterface& tab);
  CopySearchJourneyTabFeature(const CopySearchJourneyTabFeature&) = delete;
  CopySearchJourneyTabFeature& operator=(const CopySearchJourneyTabFeature&) =
      delete;
  ~CopySearchJourneyTabFeature() override;

  static CopySearchJourneyTabFeature* From(tabs::TabInterface* tab);

  // content::WebContentsObserver:
  void OnTextCopiedToClipboard(content::RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);
  void NotifyTabDestroyed(content::WebContents* contents);

  raw_ptr<CopySearchJourneyTracker> tracker_ = nullptr;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  base::CallbackListSubscription will_discard_contents_subscription_;
  ui::ScopedUnownedUserData<CopySearchJourneyTabFeature>
      scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_COPY_SEARCH_JOURNEY_TAB_FEATURE_H_

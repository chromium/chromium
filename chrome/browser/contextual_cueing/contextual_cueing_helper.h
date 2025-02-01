// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class OptimizationGuideKeyedService;

namespace tabs {
class GlicNudgeController;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService;

class ContextualCueingHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualCueingHelper> {
 public:
  // Creates ContextualCueingHelper and attaches it the `web_contents` if
  // contextual cueing is enabled.
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ContextualCueingHelper(const ContextualCueingHelper&) = delete;
  ContextualCueingHelper& operator=(const ContextualCueingHelper&) = delete;
  ~ContextualCueingHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  tabs::GlicNudgeController* GetGlicNudgeController();

  const std::string& last_navigation_cue_label() const {
    return last_navigation_cue_label_;
  }

 private:
  ContextualCueingHelper(content::WebContents* contents,
                         OptimizationGuideKeyedService* ogks,
                         ContextualCueingService* ccs);

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<ContextualCueingService> contextual_cueing_service_ = nullptr;

  // Holds the cue label for the last navigation in `this`.
  std::string last_navigation_cue_label_;

  friend WebContentsUserData<ContextualCueingHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

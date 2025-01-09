// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace contextual_cueing {

class ContextualCueingHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualCueingHelper> {
 public:
  // Creates ContextualCueingHelper and attaches it the `web_contents` if
  // contextual cueing is enabled.
  [[nodiscard]] static std::unique_ptr<ContextualCueingHelper>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  ContextualCueingHelper(const ContextualCueingHelper&) = delete;
  ContextualCueingHelper& operator=(const ContextualCueingHelper&) = delete;
  ~ContextualCueingHelper() override;

  // content::WebContentsObserver
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  ContextualCueingHelper(content::WebContents* contents,
                         optimization_guide::OptimizationGuideDecider* decider);

  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  friend WebContentsUserData<ContextualCueingHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

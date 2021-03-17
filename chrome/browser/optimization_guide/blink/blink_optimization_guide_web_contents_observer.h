// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/optimization_guide/optimization_guide.mojom.h"

class Profile;

namespace optimization_guide {

class BlinkOptimizationGuideInquirer;

// BlinkOptimizationGuideWebContentsObserver observes navigation events, queries
// the optimization guide service about optimization hints for Blink, and then
// sends them to a renderer immediately before navigation commit.
//
// An instance of this class is attached to WebContents as WebContentsUserData.
//
// TODO(https://crbug.com/1113980): This class name could be confusing with
// OptimizationGuideWebContentsObserver that is used for the optimization guide
// system itself. We should rename this to a clearer name.
class BlinkOptimizationGuideWebContentsObserver final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          BlinkOptimizationGuideWebContentsObserver> {
 public:
  BlinkOptimizationGuideWebContentsObserver(
      const BlinkOptimizationGuideWebContentsObserver&) = delete;
  BlinkOptimizationGuideWebContentsObserver& operator=(
      const BlinkOptimizationGuideWebContentsObserver&) = delete;
  BlinkOptimizationGuideWebContentsObserver(
      BlinkOptimizationGuideWebContentsObserver&&) = delete;
  BlinkOptimizationGuideWebContentsObserver& operator=(
      BlinkOptimizationGuideWebContentsObserver&&) = delete;

  ~BlinkOptimizationGuideWebContentsObserver() override;

  // content::WebContentsObserver implementation:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  BlinkOptimizationGuideInquirer* current_inquirer() {
    return current_inquirer_.get();
  }

 private:
  // These are used for WebContentsUserData.
  friend class content::WebContentsUserData<
      BlinkOptimizationGuideWebContentsObserver>;
  explicit BlinkOptimizationGuideWebContentsObserver(
      content::WebContents* web_contents);
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  Profile* const profile_;

  // Reset every time the main frame navigation gets ready to commit.
  std::unique_ptr<BlinkOptimizationGuideInquirer> current_inquirer_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_BLINK_BLINK_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_

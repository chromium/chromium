// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace contextual_cueing {
class COMPONENT_EXPORT(CONTEXTUAL_CUEING) ContextualCueingHelper
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
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit ContextualCueingHelper(content::WebContents* contents);

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace contextual_cueing {

ContextualCueingHelper::ContextualCueingHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingHelper>(*web_contents) {}

ContextualCueingHelper::~ContextualCueingHelper() = default;

// content::WebContentsObserver
void ContextualCueingHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {}

// static
std::unique_ptr<ContextualCueingHelper>
ContextualCueingHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing)) {
    return nullptr;
  }
  return base::WrapUnique<ContextualCueingHelper>(
      new ContextualCueingHelper(web_contents));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingHelper);

}  // namespace contextual_cueing

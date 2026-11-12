// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prewarm_tab_helper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace {

bool ShouldPrewarmOnPageLoadAndTabSwitchAfterPaint() {
  return base::FeatureList::IsEnabled(features::kPrewarm) &&
         base::FeatureList::IsEnabled(
             features::kPrewarmPageLoadAndTabSwitchAfterPaintTrigger) &&
         !base::FeatureList::IsEnabled(features::kPrewarmZeroSuggestTrigger);
}

}  // namespace

// static
std::unique_ptr<PrewarmTabHelper> PrewarmTabHelper::MaybeCreate(
    tabs::TabInterface& tab,
    content::WebContents* web_contents) {
  if (!ShouldPrewarmOnPageLoadAndTabSwitchAfterPaint()) {
    return nullptr;
  }
  return base::WrapUnique(new PrewarmTabHelper(tab, web_contents));
}

PrewarmTabHelper::PrewarmTabHelper(tabs::TabInterface& tab,
                                   content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), tab_(tab) {}

PrewarmTabHelper::~PrewarmTabHelper() = default;

void PrewarmTabHelper::DidFirstVisuallyNonEmptyPaint() {
  MaybeTriggerPrewarm();
}

void PrewarmTabHelper::DidStopLoading() {
  MaybeTriggerPrewarm();
}

void PrewarmTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    MaybeTriggerPrewarm();
  }
}

void PrewarmTabHelper::MaybeTriggerPrewarm() {
  if (!tab_->IsInNormalWindow()) {
    return;
  }
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE &&
      !web_contents()->IsLoading() &&
      web_contents()->CompletedFirstVisuallyNonEmptyPaint()) {
    PrerenderManager::GetOrCreateForWebContents(web_contents())
        ->MaybeStartPrewarmSearchResult();
  }
}

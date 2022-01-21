// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "chrome/browser/prerender/prerender_utils.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "content/public/browser/page.h"

PrerenderManager::~PrerenderManager() = default;

void PrerenderManager::PrimaryPageChanged(content::Page& page) {
  prerender_handle_.reset();
}

void PrerenderManager::Start(const GURL& prerendering_url,
                             TriggerReason reason) {
  // Currently some prerenders bypass this mechanism, and this class only
  // handles search suggestions.
  // TODO(https://crbug.com/1278634): Make AutocompleteActionPredictor trigger
  // prerendering via PrerenderManager, to ensure all prerenders are controlled
  // in the same place.
  DCHECK_EQ(TriggerReason::kSearchSuggestion, reason);
  if (prerender_handle_ &&
      prerender_handle_->GetInitialPrerenderingUrl() == prerendering_url) {
    return;
  }
  prerender_handle_.reset();
  prerender_handle_ = web_contents()->StartPrerendering(
      prerendering_url, content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kDefaultSearchEngineMetricSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
}

PrerenderManager::PrerenderManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrerenderManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrerenderManager);

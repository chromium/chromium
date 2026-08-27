// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preloading_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace preloading_utils {

// If you add a new type of preloading trigger, please refer to the internal
// document go/update-prerender-new-trigger-metrics to make sure that metrics
// include the newly added trigger type.
// LINT.IfChange
const char kBookmarkBarMetricSuffix[] = "BookmarkBar";
const char kNewTabPageMetricSuffix[] = "NewTabPage";
// LINT.ThenChange()

bool ShouldAllowPrefetchRedirection(
    content::BrowserContext& browser_context,
    const GURL& url,
    const std::string& embedder_histogram_suffix) {
  // This function is only interested in specific triggers. The related triggers
  // don't generate parameters to be identified by search results providers, so
  // the triggering search related urls is avoided. See crbug.com/40282403 for
  // more details.
  if (embedder_histogram_suffix != kBookmarkBarMetricSuffix &&
      embedder_histogram_suffix != kNewTabPageMetricSuffix) {
    return true;
  }
  Profile* profile = Profile::FromBrowserContext(&browser_context);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  return !((template_url_service &&
            template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
                url)) ||
           google_util::IsGoogleSearchUrl(url));
}

}  // namespace preloading_utils

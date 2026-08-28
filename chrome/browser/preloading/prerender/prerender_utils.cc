// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_utils.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace prerender_utils {

// If you add a new type of prerender trigger, please refer to the internal
// document go/update-prerender-new-trigger-metrics to make sure that metrics
// include the newly added trigger type.
// LINT.IfChange
const char kPrewarmDefaultSearchEngineMetricSuffix[] =
    "PrewarmDefaultSearchEngine";
// TODO(crbug.com/394213503): Move this to `preloading_utils`.
const char kDefaultSearchEngineMetricSuffix[] = "DefaultSearchEngine";
const char kDirectUrlInputMetricSuffix[] = "DirectURLInput";
// LINT.ThenChange()

bool IsPrewarmUrl(const GURL& url, const url::Origin& dse_origin) {
  const GURL prewarm_url = ChromeContentBrowserClient::GetPrewarmUrl();
  return prewarm_url.is_valid() && url == prewarm_url &&
         dse_origin.IsSameOriginWith(prewarm_url);
}

bool IsDefaultSearchEngine(Profile* profile, const GURL& url) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return false;
  }

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search_engine) {
    return false;
  }

  if (template_url_service->IsSearchResultsPageFromDefaultSearchProvider(url)) {
    return true;
  }

  return IsPrewarmUrl(url,
                      template_url_service->GetDefaultSearchProviderOrigin());
}

bool ShouldReuseAnyExistingProcessForNewMainFrameSiteInstance(
    content::BrowserContext* browser_context,
    const GURL& site_instance_original_url) {
  // When `kProcessPerSiteForDSE` is disabled,
  // `ProcessPerSiteUpToMainFrameThreshold` can be used for any site.
  if (!base::FeatureList::IsEnabled(features::kProcessPerSiteForDSE)) {
    return true;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile);

  return IsDefaultSearchEngine(profile, site_instance_original_url);
}

}  // namespace prerender_utils

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_

class GURL;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

// This file is used to manage some static functions and constants for
// prerender2. Some typical cases can be:
// * Indicates whether a prerender2-related feature is enabled.
// * Stores the constants to avoid hardcoded strings.
namespace prerender_utils {

// LINT.IfChange(PreloadingEmbedderTriggerType)
extern const char kPrewarmDefaultSearchEngineMetricSuffix[];
extern const char kDefaultSearchEngineMetricSuffix[];
extern const char kDirectUrlInputMetricSuffix[];
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/histograms.xml:PagePreloadingTriggerType, //tools/metrics/histograms/metadata/page/histograms.xml:PagePreloadingTriggerType)

bool IsPrewarmUrl(const GURL& url, const url::Origin& dse_origin);

bool IsDefaultSearchEngine(Profile* profile, const GURL& url);

bool ShouldReuseAnyExistingProcessForNewMainFrameSiteInstance(
    content::BrowserContext* browser_context,
    const GURL& site_instance_original_url);

}  // namespace prerender_utils

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_UTILS_H_

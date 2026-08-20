// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/back_forward_cache/back_forward_cache_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace chrome_back_forward_cache {

bool ShouldPrioritizeForBackForwardCache(
    content::BrowserContext* browser_context,
    const GURL& url) {
  if (!browser_context) {
    return false;
  }
  return TemplateURLServiceFactory::GetForProfile(
             Profile::FromBrowserContext(browser_context))
      ->IsSearchResultsPageFromDefaultSearchProvider(url);
}

}  // namespace chrome_back_forward_cache

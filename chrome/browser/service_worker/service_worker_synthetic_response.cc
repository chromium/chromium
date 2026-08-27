// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_worker/service_worker_synthetic_response.h"

#include "base/check.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace chrome_service_worker {

bool IsServiceWorkerSyntheticResponseAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->IsSystemProfile()) {
    // Exclude if the profile is a system profile.
    return false;
  }

  if (!prerender_utils::IsDefaultSearchEngine(profile, url)) {
    return false;
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  CHECK(template_url_service);
  const url::Origin dse_origin =
      template_url_service->GetDefaultSearchProviderOrigin();

  // The synthetic registration is created for `url`'s origin. Restrict it to
  // the default search provider's own origin so that alternate URLs on other
  // origins don't get a synthetic registration.
  if (!dse_origin.IsSameOriginWith(url)) {
    return false;
  }

  // Prewarm page can be treated as a DSE. As we don't want to enable synthetic
  // response on the prewarm page, manually exclude it.
  if (prerender_utils::IsPrewarmUrl(url, dse_origin)) {
    return false;
  }

  return true;
}

}  // namespace chrome_service_worker

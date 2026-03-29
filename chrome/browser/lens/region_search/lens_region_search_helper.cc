// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/region_search/lens_region_search_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace lens {

bool IsRegionSearchEnabled(BrowserWindowInterface* browser,
                           Profile* profile,
                           TemplateURLService* service,
                           const GURL& url) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  if (!browser)
    return false;
  if (!service)
    return false;

// Region selection is broken in PWAs on Mac b/250074889
#if BUILDFLAG(IS_MAC)
  if (IsInProgressiveWebApp(browser))
    return false;
#endif  // BUILDFLAG(IS_MAC)

  const TemplateURL* provider = service->GetDefaultSearchProvider();
  const bool provider_supports_image_search =
      provider && !provider->image_url().empty() &&
      provider->image_url_ref().IsValid(service->search_terms_data());
  return base::FeatureList::IsEnabled(lens::features::kLensStandalone) &&
         provider_supports_image_search &&
         !url.SchemeIs(content::kChromeUIScheme) &&
         profile->GetPrefs()->GetBoolean(prefs::kLensRegionSearchEnabled);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
}

bool IsInProgressiveWebApp(BrowserWindowInterface* browser) {
  return browser &&
         (browser->GetType() == BrowserWindowInterface::TYPE_APP ||
          browser->GetType() == BrowserWindowInterface::TYPE_APP_POPUP);
}

}  // namespace lens

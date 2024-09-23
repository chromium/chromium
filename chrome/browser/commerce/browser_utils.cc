// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/browser_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace commerce {
const int kProductSpecificationsMinTabsCount = 2;
const std::vector<GURL> GetListOfProductSpecsEligibleUrls(
    const std::vector<content::WebContents*> web_contents_list) {
  std::vector<GURL> urls;
  for (auto* wc : web_contents_list) {
    auto url = wc->GetURL();
    if (!url.SchemeIs(url::kHttpsScheme) && !url.SchemeIs(url::kHttpScheme)) {
      continue;
    }

    urls.push_back(url);
  }
  return urls;
}

bool IsWebContentsListEligibleForProductSpecs(
    const std::vector<content::WebContents*> web_contents_list) {
  auto eligible_urls = GetListOfProductSpecsEligibleUrls(web_contents_list);
  return static_cast<int>(eligible_urls.size()) >=
         kProductSpecificationsMinTabsCount;
}

bool IsProductSpecsMultiSelectMenuEnabled(Profile* profile,
                                          content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(commerce::kProductSpecifications) ||
      !profile || !web_contents) {
    return false;
  }

  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    return false;
  }

  auto url = web_contents->GetURL();
  if (!url.SchemeIs(url::kHttpsScheme) && !url.SchemeIs(url::kHttpScheme)) {
    return false;
  }

  return true;
}
}  // namespace commerce

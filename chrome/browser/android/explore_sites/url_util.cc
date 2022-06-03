// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/url_util.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "url/gurl.h"

namespace explore_sites {

GURL GetBaseURL() {
  const char kBaseURLOption[] = "base_url";
  const char kDefaultBaseUrl[] = "https://exploresites-pa.googleapis.com";
  std::string field_trial_param = base::GetFieldTrialParamValueByFeature(
      chrome::android::kExploreSites, kBaseURLOption);
  if (field_trial_param.empty())
    return GURL(kDefaultBaseUrl);
  return GURL(field_trial_param);
}

GURL GetCatalogURL() {
  const char kGetCatalogPath[] = "/v1/getcatalog";
  std::string path(kGetCatalogPath);

  GURL base_url(GetBaseURL());
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}

GURL GetCategoriesURL() {
  const char kNtpJsonPath[] = "/v1/getcategories";
  std::string path(kNtpJsonPath);

  GURL base_url(GetBaseURL());
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}

}  // namespace explore_sites

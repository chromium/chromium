// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/kids_management_api.h"

#include "base/feature_list.h"
#include "components/variations/variations_associated_data.h"
#include "url/gurl.h"

namespace kids_management_api {

namespace {

const char kDefaultBaseURL[] =
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/";

// A dummy feature that can be used to specify a variation param that overrides
// the default API URL.
BASE_FEATURE(kKidsManagementAPIFeature,
             "KidsManagementAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kURLParamName[] = "kids_management_api_url";

}  // namespace

GURL GetBaseURL() {
  // If the parameter isn't set or the feature is disabled, this will return
  // the empty string, resulting in an invalid URL.
  GURL url(variations::GetVariationParamValueByFeature(
      kKidsManagementAPIFeature, kURLParamName));
  if (url.is_valid())
    return url;
  return GURL(kDefaultBaseURL);
}

GURL GetURL(const std::string& path) {
  return GetBaseURL().Resolve(path);
}

}  // namespace kids_management_api

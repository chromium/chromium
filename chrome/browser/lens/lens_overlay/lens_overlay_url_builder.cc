// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"

#include "components/lens/lens_features.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace lens {
namespace {
// Query parameter for the search text query.
inline constexpr char kTextQueryParameterKey[] = "q";

// Query parameter for denoting a search companion request.
inline constexpr char kSearchCompanionParameterKey[] = "gsc";
inline constexpr char kSearchCompanionParameterValue[] = "1";

// Query parameter for denoting an ambient request source.
inline constexpr char kAmbientParameterKey[] = "masfc";
inline constexpr char kAmbientParameterValue[] = "c";

}  // namespace

GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify) {
  GURL new_url = url_to_modify;
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kSearchCompanionParameterKey, kSearchCompanionParameterValue);
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kAmbientParameterKey, kAmbientParameterValue);
  return new_url;
}

GURL BuildSearchURL(const std::string& text_query) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey, text_query);
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params);
  return url_with_query_params;
}
}  // namespace lens

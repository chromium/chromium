// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"

#include "base/base64url.h"
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

// Query parameter for the search session id.
inline constexpr char kSearchSessionIdParameterKey[] = "gsessionid";

// Query parameter for the request id.
inline constexpr char kRequestIdParameterKey[] = "vsrid";

// Query parameter for the mode.
inline constexpr char kModeParameterKey[] = "udm";
// Query parameter values for the mode.
inline constexpr char kUnimodalModeParameterValue[] = "26";
inline constexpr char kMultimodalModeParameterValue[] = "24";

}  // namespace

GURL AppendCommonSearchParametersToURL(const GURL& url_to_modify) {
  GURL new_url = url_to_modify;
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kSearchCompanionParameterKey, kSearchCompanionParameterValue);
  new_url = net::AppendOrReplaceQueryParameter(
      new_url, kAmbientParameterKey, kAmbientParameterValue);
  return new_url;
}

GURL BuildTextOnlySearchURL(const std::string& text_query) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey, text_query);
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params);
  return url_with_query_params;
}

GURL BuildLensSearchURL(std::optional<std::string> text_query,
                        std::unique_ptr<lens::LensOverlayRequestId> request_id,
                        lens::LensOverlayClusterInfo cluster_info) {
  GURL url_with_query_params =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  url_with_query_params =
      AppendCommonSearchParametersToURL(url_with_query_params);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kTextQueryParameterKey,
      text_query.has_value() ? *text_query : "");

  // The search url should use the search session id from the cluster info.
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kSearchSessionIdParameterKey,
      cluster_info.search_session_id());

  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kModeParameterKey,
      text_query.has_value() ? kMultimodalModeParameterValue
                             : kUnimodalModeParameterValue);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kRequestIdParameterKey, encoded_request_id);

  return url_with_query_params;
}

}  // namespace lens

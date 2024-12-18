// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mall/mall_url.h"

#include "ash/webui/mall/url_constants.h"
#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace ash {

BASE_FEATURE_PARAM(int,
                   kMallAlmanacFeatureFlag,
                   &chromeos::features::kCrosMall,
                   "almanac_flag_id",
                   0);

GURL GetMallLaunchUrl(const apps::DeviceInfo& info, std::string_view path) {
  apps::proto::ClientContext context;
  *context.mutable_device_context() = info.ToDeviceContext();
  *context.mutable_user_context() = info.ToUserContext();

  // Mall experiments may define an Almanac feature flag which we attach as part
  // of the client context, so that it is included in requests to Almanac.
  if (kMallAlmanacFeatureFlag.Get() != 0) {
    context.mutable_user_context()->add_flag_ids(kMallAlmanacFeatureFlag.Get());
  }

  std::string encoded_context = base::Base64Encode(context.SerializeAsString());

  constexpr std::string_view kContextParameter = "context";

  GURL::Replacements replacements;
  replacements.SetPathStr(path);

  GURL url_with_path = GetMallBaseUrl().ReplaceComponents(replacements);
  if (!url_with_path.is_valid()) {
    url_with_path = GetMallBaseUrl();
  }

  return net::AppendOrReplaceQueryParameter(url_with_path, kContextParameter,
                                            encoded_context);
}

}  // namespace ash

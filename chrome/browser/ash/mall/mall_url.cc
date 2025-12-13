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
#include "chromeos/constants/url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace ash {

GURL GetMallLaunchUrl(const apps::DeviceInfo& info, std::string_view path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(path);

  GURL url = GURL(chromeos::kAppMallBaseUrl).ReplaceComponents(replacements);
  if (!url.is_valid()) {
    url = GURL(chromeos::kAppMallBaseUrl);
  }

  // Append the parent frame origin for the server to allow iframing for this
  // origin.
  constexpr std::string_view kOriginParameter = "origin";
  url = net::AppendOrReplaceQueryParameter(url, kOriginParameter,
                                           kChromeUIMallUrl);

  // Append context for localization of app recommendations.
  constexpr std::string_view kContextParameter = "context";
  apps::proto::ClientContext context;
  *context.mutable_device_context() = info.ToDeviceContext();
  *context.mutable_user_context() = info.ToUserContext();
  std::string encoded_context = base::Base64Encode(context.SerializeAsString());
  return net::AppendOrReplaceQueryParameter(url, kContextParameter,
                                            encoded_context);
}

}  // namespace ash

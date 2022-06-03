// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/signin/reauth_util.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"

namespace signin {

GURL GetReauthConfirmationURL(signin_metrics::ReauthAccessPoint access_point) {
  GURL url = GURL(chrome::kChromeUISigninReauthURL);
  url = net::AppendQueryParameter(
      url, "access_point",
      base::NumberToString(static_cast<int>(access_point)));
  return url;
}

signin_metrics::ReauthAccessPoint GetReauthAccessPointForReauthConfirmationURL(
    const GURL& url) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, "access_point", &value))
    return signin_metrics::ReauthAccessPoint::kUnknown;

  int access_point = -1;
  base::StringToInt(value, &access_point);
  if (access_point <=
          static_cast<int>(signin_metrics::ReauthAccessPoint::kUnknown) ||
      access_point >
          static_cast<int>(signin_metrics::ReauthAccessPoint::kMaxValue)) {
    return signin_metrics::ReauthAccessPoint::kUnknown;
  }

  return static_cast<signin_metrics::ReauthAccessPoint>(access_point);
}

}  // namespace signin

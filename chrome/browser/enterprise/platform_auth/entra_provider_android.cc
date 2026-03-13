// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/entra_provider_android.h"

#include "base/containers/fixed_flat_set.h"

namespace enterprise_auth {

namespace {

static constexpr auto kSupportedOrigins =
    base::MakeFixedFlatSet<std::string_view>(
        {"https://login.microsoftonline.com", "https://login.microsoft.com",
         "https://login.windows.net", "https://login.microsoftonline.us",
         "https://login.partner.microsoftonline.cn"});
}

bool EntraProviderAndroid::SupportsOriginFiltering() {
  return true;
}

void EntraProviderAndroid::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  auto origins = std::make_unique<std::vector<url::Origin>>();
  for (const std::string_view origin : kSupportedOrigins) {
    origins->push_back(url::Origin::Create(GURL(origin)));
  }
  std::move(on_fetch_complete).Run(std::move(origins));
}

void EntraProviderAndroid::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  // TODO: b/484014627 - fetch the headers from Android.
  std::move(callback).Run({});
}

}  // namespace enterprise_auth

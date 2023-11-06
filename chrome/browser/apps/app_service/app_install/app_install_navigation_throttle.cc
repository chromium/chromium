// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"

#include <memory>
#include <string_view>

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "url/url_util.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

using std::literals::string_view_literals::operator""sv;

// For matching URIs of the form: almanac://install-app?package_id=<package id>
constexpr std::string_view kAppInstallScheme = "almanac"sv;
constexpr std::string_view kAppInstallPath = "//install-app"sv;
constexpr std::string_view kAppInstallPackageIdParam = "package_id"sv;

constexpr size_t kMaxDecodeLength = 2048;

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
AppInstallNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (chromeos::features::IsAppInstallServiceUriEnabled()) {
    return std::make_unique<apps::AppInstallNavigationThrottle>(handle);
  }
  return nullptr;
}

// static
absl::optional<PackageId> AppInstallNavigationThrottle::ExtractPackageId(
    std::string_view query) {
  url::Component query_slice(0, query.length());
  url::Component key_slice;
  url::Component value_slice;
  while (url::ExtractQueryKeyValue(query.begin(), &query_slice, &key_slice,
                                   &value_slice)) {
    if (query.substr(key_slice.begin, key_slice.len) !=
        kAppInstallPackageIdParam) {
      continue;
    }

    url::RawCanonOutputW<kMaxDecodeLength> decoded_value;
    url::DecodeURLEscapeSequences(
        query.substr(value_slice.begin, value_slice.len),
        url::DecodeURLMode::kUTF8OrIsomorphic, &decoded_value);

    // TODO(b/299825321): Make DecodeURLEscapeSequences() work with
    // RawCanonOutput to avoid this redundant UTF8 -> UTF16 -> UTF8
    // conversion.
    return PackageId::FromString(base::UTF16ToUTF8(decoded_value.view()));
  }
  return absl::nullopt;
}

AppInstallNavigationThrottle::AppInstallNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {
  CHECK(chromeos::features::IsAppInstallServiceUriEnabled());
}

AppInstallNavigationThrottle::~AppInstallNavigationThrottle() = default;

const char* AppInstallNavigationThrottle::GetNameForLogging() {
  return "AppInstallNavigationThrottle";
}

ThrottleCheckResult AppInstallNavigationThrottle::WillStartRequest() {
  return HandleRequest();
}

// TODO(b/299825321): Make this require redirection from an Almanac server.
ThrottleCheckResult AppInstallNavigationThrottle::WillRedirectRequest() {
  return HandleRequest();
}

ThrottleCheckResult AppInstallNavigationThrottle::HandleRequest() {
  // TODO(b/304680258): Integration test this flow.
  const GURL& url = navigation_handle()->GetURL();
  if (url.SchemeIs(kAppInstallScheme) && url.path_piece() == kAppInstallPath) {
    absl::optional<PackageId> package_id = ExtractPackageId(url.query_piece());
    // TODO(b/303350800): Generalize to work with all app types.
    if (package_id.has_value() && package_id->app_type() == AppType::kWeb) {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext());
      auto* proxy = AppServiceProxyFactory::GetForProfile(profile);
      proxy->AppInstallService().InstallApp(package_id.value());
    }

    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace apps

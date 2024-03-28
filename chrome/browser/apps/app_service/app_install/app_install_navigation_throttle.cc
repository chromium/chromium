// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"

#include <memory>
#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/url_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "url/url_util.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

constexpr std::string_view kAppInstallPath = "//install-app";
constexpr std::string_view kAppInstallPackageIdParam = "package_id";
constexpr std::string_view kAppInstallSourceParam = "source";

constexpr size_t kMaxDecodeLength = 2048;

AppInstallSurface SourceParamToAppInstallSurface(std::string_view source) {
  if (base::EqualsCaseInsensitiveASCII(source, "showoff")) {
    return AppInstallSurface::kAppInstallUriShowoff;
  }
  if (base::EqualsCaseInsensitiveASCII(source, "mall")) {
    return AppInstallSurface::kAppInstallUriMall;
  }
  if (base::EqualsCaseInsensitiveASCII(source, "getit")) {
    return AppInstallSurface::kAppInstallUriGetit;
  }
  if (base::EqualsCaseInsensitiveASCII(source, "launcher")) {
    return AppInstallSurface::kAppInstallUriLauncher;
  }
  return AppInstallSurface::kAppInstallUriUnknown;
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  switch (handle->GetNavigationInitiatorActivationAndAdStatus()) {
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kDidNotStartWithTransientActivation:
      return false;
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromNonAd:
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromAd:
      return true;
  }
}

}  // namespace

// static
base::OnceCallback<void(bool created)>&
AppInstallNavigationThrottle::MaybeCreateCallbackForTesting() {
  static base::NoDestructor<base::OnceCallback<void(bool created)>> callback;
  return *callback;
}

// static
std::unique_ptr<content::NavigationThrottle>
AppInstallNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  std::unique_ptr<content::NavigationThrottle> throttle;
  if (chromeos::features::IsAppInstallServiceUriEnabled() &&
      IsNavigationUserInitiated(handle)) {
    throttle = std::make_unique<apps::AppInstallNavigationThrottle>(handle);
  }

  if (MaybeCreateCallbackForTesting()) {
    std::move(MaybeCreateCallbackForTesting()).Run(static_cast<bool>(throttle));
  }

  return throttle;
}

AppInstallNavigationThrottle::QueryParams::QueryParams() = default;

AppInstallNavigationThrottle::QueryParams::QueryParams(
    std::optional<PackageId> package_id,
    AppInstallSurface source)
    : package_id(std::move(package_id)), source(source) {}

AppInstallNavigationThrottle::QueryParams::QueryParams(QueryParams&&) = default;

AppInstallNavigationThrottle::QueryParams::~QueryParams() = default;

bool AppInstallNavigationThrottle::QueryParams::operator==(
    const QueryParams& other) const {
  return package_id == other.package_id && source == other.source;
}

// static
AppInstallNavigationThrottle::QueryParams
AppInstallNavigationThrottle::ExtractQueryParams(std::string_view query) {
  QueryParams result;
  url::Component query_slice(0, query.length());
  url::Component key_slice;
  url::Component value_slice;
  while (url::ExtractQueryKeyValue(query, &query_slice, &key_slice,
                                   &value_slice)) {
    std::string_view key = query.substr(key_slice.begin, key_slice.len);

    auto decode_value = [&]() {
      url::RawCanonOutputW<kMaxDecodeLength> decoded_value;
      url::DecodeURLEscapeSequences(
          query.substr(value_slice.begin, value_slice.len),
          url::DecodeURLMode::kUTF8OrIsomorphic, &decoded_value);

      // TODO(b/299825321): Make DecodeURLEscapeSequences() work with
      // RawCanonOutput to avoid this redundant UTF8 -> UTF16 -> UTF8
      // conversion.
      return base::UTF16ToUTF8(decoded_value.view());
    };

    if (key == kAppInstallPackageIdParam) {
      result.package_id = PackageId::FromString(decode_value());
    } else if (key == kAppInstallSourceParam) {
      result.source = SourceParamToAppInstallSurface(decode_value());
    }
  }
  return result;
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

ThrottleCheckResult AppInstallNavigationThrottle::WillRedirectRequest() {
  return HandleRequest();
}

ThrottleCheckResult AppInstallNavigationThrottle::HandleRequest() {
  const GURL& url = navigation_handle()->GetURL();
  if (!url.SchemeIs(chromeos::kAppInstallUriScheme) ||
      url.path_piece() != kAppInstallPath) {
    return content::NavigationThrottle::PROCEED;
  }

  QueryParams query_params = ExtractQueryParams(url.query_piece());
  // TODO(b/303350800): Generalize to work with all app types.
  if (!query_params.package_id.has_value() ||
      (query_params.package_id->app_type() != AppType::kWeb &&
       query_params.package_id->app_type() != AppType::kBorealis)) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  Profile* profile = Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);
  proxy->AppInstallService().InstallApp(
      query_params.source, std::move(query_params.package_id.value()),
      base::DoNothing());

  if (!chromeos::features::IsCrosWebAppInstallDialogEnabled() &&
      LinkCapturingNavigationThrottle::
          IsEmptyDanglingWebContentsAfterLinkCapture(navigation_handle())) {
    navigation_handle()->GetWebContents()->Close();
  }

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace apps

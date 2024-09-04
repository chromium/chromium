// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"

#include <memory>
#include <optional>
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
// TODO(crbug.com/40251079): Remove circular includes.
#include "chrome/browser/ui/browser_finder.h"  // nogncheck
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/unguessable_token.h"
#include "ui/gfx/native_widget_types.h"
#endif

static_assert(BUILDFLAG(IS_CHROMEOS));

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
constexpr char kAppInstallParentWindowFound[] =
    "Apps.AppInstallParentWindowFound";
#endif

constexpr std::string_view kAppInstallHost = "install-app";
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
  if (base::EqualsCaseInsensitiveASCII(source, "mallv2")) {
    return AppInstallSurface::kAppInstallUriMallV2;
  }
  if (base::EqualsCaseInsensitiveASCII(source, "getit")) {
    return AppInstallSurface::kAppInstallUriGetit;
  }
  if (base::EqualsCaseInsensitiveASCII(source, "launcher")) {
    return AppInstallSurface::kAppInstallUriLauncher;
  }
  if (base::EqualsCaseInsensitiveASCII(source, "peripherals")) {
    return AppInstallSurface::kAppInstallUriPeripherals;
  }
  return AppInstallSurface::kAppInstallUriUnknown;
}

// Retrieves an identifier to the window we are anchoring to.
std::optional<AppInstallService::WindowIdentifier> GetAnchorWindow(
    content::WebContents* web_contents,
    AppServiceProxy* proxy) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return web_contents->GetTopLevelNativeWindow();
#else
  static_assert(BUILDFLAG(IS_CHROMEOS_LACROS));

  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  if (!browser) {
    return std::nullopt;
  }

  CHECK(proxy->BrowserAppInstanceTracker());

  const BrowserWindowInstance* browser_window =
      proxy->BrowserAppInstanceTracker()->GetBrowserWindowInstance(browser);

  const BrowserAppInstance* browser_app =
      proxy->BrowserAppInstanceTracker()->GetAppInstance(browser);

  if (browser_window) {
    base::UmaHistogramBoolean(kAppInstallParentWindowFound, true);
    return browser_window->id;
  }

  if (browser_app) {
    base::UmaHistogramBoolean(kAppInstallParentWindowFound, true);
    return browser_app->id;
  }

  // Unexpected to reach here, as the origin of the install dialog must be a
  // browser or app window. However, BrowserAppInstanceTracker is operating on
  // async window changes over crosapi, so the anchor window might not be
  // tracked at this point. In this case the dialog will not be anchored to any
  // parent.
  DLOG(WARNING) << "App Install Dialog parent not found.";
  base::UmaHistogramBoolean(kAppInstallParentWindowFound, false);
  return std::nullopt;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  if (!handle->IsRendererInitiated()) {
    return true;
  }

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
  if (IsNavigationUserInitiated(handle)) {
    throttle = std::make_unique<apps::AppInstallNavigationThrottle>(handle);
  }

  if (MaybeCreateCallbackForTesting()) {
    std::move(MaybeCreateCallbackForTesting()).Run(static_cast<bool>(throttle));
  }

  return throttle;
}

AppInstallNavigationThrottle::QueryParams::QueryParams() = default;

AppInstallNavigationThrottle::QueryParams::QueryParams(
    std::optional<std::string> serialized_package_id,
    AppInstallSurface source)
    : serialized_package_id(std::move(serialized_package_id)), source(source) {}

AppInstallNavigationThrottle::QueryParams::QueryParams(QueryParams&&) = default;

AppInstallNavigationThrottle::QueryParams::~QueryParams() = default;

bool AppInstallNavigationThrottle::QueryParams::operator==(
    const QueryParams& other) const {
  return serialized_package_id == other.serialized_package_id &&
         source == other.source;
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
      std::string serialized_package_id = decode_value();
      if (!serialized_package_id.empty()) {
        result.serialized_package_id = std::move(serialized_package_id);
      }
    } else if (key == kAppInstallSourceParam) {
      result.source = SourceParamToAppInstallSurface(decode_value());
    }
  }
  return result;
}

AppInstallNavigationThrottle::AppInstallNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

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

  if (!url.SchemeIs(chromeos::kAppInstallUriScheme) &&
      !url.SchemeIs(chromeos::kLegacyAppInstallUriScheme)) {
    return content::NavigationThrottle::PROCEED;
  }

  // We accept `cros-apps:install-app` or `cros-apps://install-app`, when parsed
  // with an opaque path (no host, path starts with //) or not.
  if (url.host() != kAppInstallHost && url.path_piece() != kAppInstallHost &&
      url.path_piece() != kAppInstallPath) {
    return content::NavigationThrottle::PROCEED;
  }

  QueryParams query_params = ExtractQueryParams(url.query_piece());
  if (!query_params.serialized_package_id.has_value()) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

  std::optional<AppInstallService::WindowIdentifier> anchor_window =
      GetAnchorWindow(web_contents, proxy);

  proxy->AppInstallService().InstallAppWithFallback(
      query_params.source,
      std::move(query_params.serialized_package_id).value(), anchor_window,
      base::DoNothing());

  if (!web_contents->GetLastCommittedURL().is_valid()) {
    web_contents->ClosePage();
  }

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace apps

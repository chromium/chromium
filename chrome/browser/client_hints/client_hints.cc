// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <string>

#include "chrome/browser/client_hints/client_hints.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/client_hints/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#include "content/public/common/page_zoom.h"
#endif  // !OS_ANDROID

namespace {

uint8_t randomization_salt = 0;

constexpr size_t kMaxRandomNumbers = 21;

// Returns the randomization salt (weak and insecure) that should be used when
// adding noise to the network quality metrics. This is known only to the
// device, and is generated only once. This makes it possible to add the same
// amount of noise for a given origin.
uint8_t RandomizationSalt() {
  if (randomization_salt == 0)
    randomization_salt = base::RandInt(1, kMaxRandomNumbers);
  DCHECK_LE(1, randomization_salt);
  DCHECK_GE(kMaxRandomNumbers, randomization_salt);
  return randomization_salt;
}

double GetRandomMultiplier(const std::string& host) {
  // The random number should be a function of the hostname to reduce
  // cross-origin fingerprinting. The random number should also be a function
  // of randomized salt which is known only to the device. This prevents
  // origin from removing noise from the estimates.
  unsigned hash = std::hash<std::string>{}(host) + RandomizationSalt();
  double random_multiplier =
      0.9 + static_cast<double>((hash % kMaxRandomNumbers)) * 0.01;
  DCHECK_LE(0.90, random_multiplier);
  DCHECK_GE(1.10, random_multiplier);
  return random_multiplier;
}

bool IsJavaScriptAllowed(Profile* profile, const GURL& url) {
  return HostContentSettingsMapFactory::GetForProfile(profile)
             ->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 std::string()) == CONTENT_SETTING_ALLOW;
}

// Returns the zoom factor for a given |url|.
double GetZoomFactor(content::BrowserContext* context, const GURL& url) {
// Android does not have the concept of zooming in like desktop.
#if defined(OS_ANDROID)
  return 1.0;
#else

  double zoom_level = content::HostZoomMap::GetDefaultForBrowserContext(context)
                          ->GetZoomLevelForHostAndScheme(
                              url.scheme(), net::GetHostOrSpecFromURL(url));

  if (zoom_level == 0.0) {
    // Get default zoom level.
    zoom_level = content::HostZoomMap::GetDefaultForBrowserContext(context)
                     ->GetDefaultZoomLevel();
  }

  return content::ZoomLevelToZoomFactor(zoom_level);
#endif
}

double GetDeviceScaleFactor() {
  double device_scale_factor = 1.0;
  if (display::Screen::GetScreen()) {
    device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  }
  DCHECK_LT(0.0, device_scale_factor);
  return device_scale_factor;
}

// Returns a string corresponding to |value|. The returned string satisfies
// ABNF: 1*DIGIT [ "." 1*DIGIT ]
std::string DoubleToSpecCompliantString(double value) {
  DCHECK_LE(0.0, value);
  std::string result = base::NumberToString(value);
  DCHECK(!result.empty());
  if (value >= 1.0)
    return result;

  DCHECK_LE(0.0, value);
  DCHECK_GT(1.0, value);

  // Check if there is at least one character before period.
  if (result.at(0) != '.')
    return result;

  // '.' is the first character in |result|. Prefix one digit before the
  // period to make it spec compliant.
  return "0" + result;
}

// Return the effective connection type value overridden for web APIs.
// If no override value has been set, a null value is returned.
base::Optional<net::EffectiveConnectionType>
GetWebHoldbackEffectiveConnectionType() {
  if (!base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    return base::nullopt;
  }
  std::string effective_connection_type_param =
      base::GetFieldTrialParamValueByFeature(
          features::kNetworkQualityEstimatorWebHoldback,
          "web_effective_connection_type_override");

  base::Optional<net::EffectiveConnectionType> effective_connection_type =
      net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
  DCHECK(effective_connection_type_param.empty() || effective_connection_type);

  if (!effective_connection_type)
    return base::nullopt;
  DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type.value());
  return effective_connection_type;
}

}  // namespace

namespace client_hints {

namespace internal {

unsigned long RoundRtt(const std::string& host,
                       const base::Optional<base::TimeDelta>& rtt) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityMsec = 50;
  static const double kMaxRttMsec = 3.0 * 1000;

  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  double rtt_msec = static_cast<double>(rtt.value().InMilliseconds());
  rtt_msec *= GetRandomMultiplier(host);
  rtt_msec = std::min(rtt_msec, kMaxRttMsec);

  DCHECK_LE(0, rtt_msec);
  DCHECK_GE(kMaxRttMsec, rtt_msec);

  // Round down to the nearest kBucketSize msec value.
  return std::round(rtt_msec / kGranularityMsec) * kGranularityMsec;
}

double RoundKbpsToMbps(const std::string& host,
                       const base::Optional<int32_t>& downlink_kbps) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityKbps = 50;
  static const double kMaxDownlinkKbps = 10.0 * 1000;

  // If downlink is unavailable, return the fastest value.
  double randomized_downlink_kbps = downlink_kbps.value_or(kMaxDownlinkKbps);
  randomized_downlink_kbps *= GetRandomMultiplier(host);

  randomized_downlink_kbps =
      std::min(randomized_downlink_kbps, kMaxDownlinkKbps);

  DCHECK_LE(0, randomized_downlink_kbps);
  DCHECK_GE(kMaxDownlinkKbps, randomized_downlink_kbps);
  // Round down to the nearest kGranularityKbps kbps value.
  double downlink_kbps_rounded =
      std::round(randomized_downlink_kbps / kGranularityKbps) *
      kGranularityKbps;

  // Convert from Kbps to Mbps.
  return downlink_kbps_rounded / 1000;
}

}  // namespace internal

std::unique_ptr<net::HttpRequestHeaders>
GetAdditionalNavigationRequestClientHintsHeaders(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));

  // Get the client hint headers.
  if (!url.is_valid())
    return nullptr;

  if (!url.SchemeIsHTTPOrHTTPS())
    return nullptr;

  if (url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url))
    return nullptr;

  DCHECK(url.SchemeIs(url::kHttpsScheme) ||
         (url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url)));

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;

  // Check if |url| is allowed to run JavaScript. If not, client hints are not
  // attached to the requests that initiate on the browser side.
  if (!IsJavaScriptAllowed(profile, url)) {
    return nullptr;
  }

  ContentSettingsForOneType client_hints_host_settings;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_host_settings);

  blink::WebEnabledClientHints web_client_hints;

  GetAllowedClientHintsFromSource(
      url /* resource url */, client_hints_host_settings, &web_client_hints);

  std::unique_ptr<net::HttpRequestHeaders> additional_headers(
      std::make_unique<net::HttpRequestHeaders>());

  // Currently, only "device-memory" client hint request header is added from
  // the browser process.
  if (web_client_hints.IsEnabled(
          blink::mojom::WebClientHintsType::kDeviceMemory)) {
    // Initialize device memory if it's not already.
    blink::ApproximatedDeviceMemory::Initialize();
    const float device_memory =
        blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
    DCHECK_LT(0.0, device_memory);
    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kDeviceMemory)],
        DoubleToSpecCompliantString(device_memory));
  }

  if (web_client_hints.IsEnabled(blink::mojom::WebClientHintsType::kDpr)) {
    double device_scale_factor = GetDeviceScaleFactor();

    double zoom_factor = GetZoomFactor(context, url);
    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kDpr)],
        DoubleToSpecCompliantString(device_scale_factor * zoom_factor));
  }

  if (web_client_hints.IsEnabled(
          blink::mojom::WebClientHintsType::kViewportWidth)) {
    // The default value on Android. See
    // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/viewportAndroid.css.
    double viewport_width = 980;

#if !defined(OS_ANDROID)
    double device_scale_factor = GetDeviceScaleFactor();
    viewport_width = (display::Screen::GetScreen()
                          ->GetPrimaryDisplay()
                          .GetSizeInPixel()
                          .width()) /
                     GetZoomFactor(context, url) / device_scale_factor;
#endif  // !OS_ANDROID
    DCHECK_LT(0, viewport_width);
    if (viewport_width > 0) {
      additional_headers->SetHeader(
          blink::kClientHintsHeaderMapping[static_cast<int>(
              blink::mojom::WebClientHintsType::kViewportWidth)],
          base::NumberToString(std::round(viewport_width)));
    }
  }

  network::NetworkQualityTracker* network_quality_tracker =
      g_browser_process->network_quality_tracker();

  if (web_client_hints.IsEnabled(blink::mojom::WebClientHintsType::kRtt)) {
    base::Optional<net::EffectiveConnectionType> web_holdback_ect =
        GetWebHoldbackEffectiveConnectionType();

    base::TimeDelta http_rtt;
    if (web_holdback_ect.has_value()) {
      http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
          web_holdback_ect.value());
    } else {
      http_rtt = network_quality_tracker->GetHttpRTT();
    }
    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kRtt)],
        base::NumberToString(internal::RoundRtt(url.host(), http_rtt)));
  }

  if (web_client_hints.IsEnabled(blink::mojom::WebClientHintsType::kDownlink)) {
    base::Optional<net::EffectiveConnectionType> web_holdback_ect =
        GetWebHoldbackEffectiveConnectionType();

    int32_t downlink_throughput_kbps;

    if (web_holdback_ect.has_value()) {
      downlink_throughput_kbps =
          net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
              web_holdback_ect.value());
    } else {
      downlink_throughput_kbps =
          network_quality_tracker->GetDownstreamThroughputKbps();
    }

    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kDownlink)],
        DoubleToSpecCompliantString(
            internal::RoundKbpsToMbps(url.host(), downlink_throughput_kbps)));
  }

  if (web_client_hints.IsEnabled(blink::mojom::WebClientHintsType::kEct)) {
    DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
              net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
    DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
              static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));

    base::Optional<net::EffectiveConnectionType> web_holdback_ect =
        GetWebHoldbackEffectiveConnectionType();

    int effective_connection_type =
        web_holdback_ect.has_value()
            ? web_holdback_ect.value()
            : static_cast<int>(
                  network_quality_tracker->GetEffectiveConnectionType());

    additional_headers->SetHeader(
        blink::kClientHintsHeaderMapping[static_cast<int>(
            blink::mojom::WebClientHintsType::kEct)],
        blink::kWebEffectiveConnectionTypeMapping[effective_connection_type]);
  }

  // Static assert that triggers if a new client hint header is added. If a
  // new client hint header is added, the following assertion should be updated.
  // If possible, logic should be added above so that the request headers for
  // the newly added client hint can be added to the request.
  static_assert(
      blink::mojom::WebClientHintsType::kEct ==
          blink::mojom::WebClientHintsType::kMaxValue,
      "Consider adding client hint request headers from the browser process");

  // TODO(crbug.com/735518): If the request is redirected, the client hint
  // headers stay attached to the redirected request. Consider removing/adding
  // the client hints headers if the request is redirected with a change in
  // scheme or a change in the origin.
  return additional_headers;
}

}  // namespace client_hints

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_discovery_metrics.h"

#include "chrome/browser/apps/app_service/metrics/app_discovery_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace apps {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

cros_events::AppInstallSurface ToCrosEventsAppInstallSurface(
    AppInstallSurface surface) {
  switch (surface) {
    case AppInstallSurface::kAppPreloadServiceOem:
      return cros_events::AppInstallSurface::APP_PRELOAD_SERVICE_OEM;
    case AppInstallSurface::kAppPreloadServiceDefault:
      return cros_events::AppInstallSurface::APP_PRELOAD_SERVICE_DEFAULT;
    case AppInstallSurface::kOobeAppRecommendations:
      return cros_events::AppInstallSurface::OOBE_APP_RECOMMENDATIONS;
    case AppInstallSurface::kAppInstallUriUnknown:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_UNKNOWN;
    case AppInstallSurface::kAppInstallUriShowoff:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_SHOWOFF;
    case AppInstallSurface::kAppInstallUriMall:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_MALL;
    case AppInstallSurface::kAppInstallUriMallV2:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_MALL_V2;
    case AppInstallSurface::kAppInstallUriGetit:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_GETIT;
    case AppInstallSurface::kAppInstallUriLauncher:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_LAUNCHER;
    case AppInstallSurface::kAppInstallUriPeripherals:
      return cros_events::AppInstallSurface::APP_INSTALL_URI_PERIPHERALS;
  }
}

}  // namespace

void RecordAppDiscoveryMetricForInstallRequest(Profile* profile,
                                               AppInstallSurface surface,
                                               const PackageId& package_id) {
  // App Discovery metrics record App IDs, and so use the same app sync consent
  // as AppKM.
  if (!ShouldRecordAppKM(profile)) {
    return;
  }

  // App metrics are allowed in MGS for system/policy installed apps only.
  // Only record metrics for Surfaces initiated by the system, like Preloads.
  if (chromeos::IsManagedGuestSession() &&
      !(surface == AppInstallSurface::kAppPreloadServiceDefault ||
        surface == AppInstallSurface::kAppPreloadServiceOem)) {
    return;
  }

  if (std::optional<std::string> metrics_app_id =
          apps::AppDiscoveryMetrics::GetAppStringToRecordForPackage(
              package_id)) {
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_AppInstallService_InstallRequested()
            .SetAppId(*metrics_app_id)
            .SetSurface(ToCrosEventsAppInstallSurface(surface)));
  }
}

}  // namespace apps

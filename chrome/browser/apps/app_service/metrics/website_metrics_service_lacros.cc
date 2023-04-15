// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"

#include "base/time/time.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace apps {

namespace {

// Interval for reporting noisy AppKM events.
constexpr base::TimeDelta kNoisyAppKMReportInterval = base::Hours(2);

// Check for app usage time, input event each 5 minutes.
constexpr base::TimeDelta kFiveMinutes = base::Minutes(5);

}  // namespace

WebsiteMetricsServiceLacros::WebsiteMetricsServiceLacros(Profile* profile)
    : profile_(profile) {}

WebsiteMetricsServiceLacros::~WebsiteMetricsServiceLacros() = default;

// static
void WebsiteMetricsServiceLacros::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kWebsiteUsageTime);
}

void WebsiteMetricsServiceLacros::InitDeviceTypeAndStart() {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::DeviceAttributes>()) {
    return;
  }

  int mojo_version =
      service->GetInterfaceVersion(crosapi::mojom::DeviceAttributes::Uuid_);
  if (mojo_version < int{crosapi::mojom::DeviceAttributes::MethodMinVersions::
                             kGetDeviceTypeForMetricsMinVersion}) {
    return;
  }

  service->GetRemote<crosapi::mojom::DeviceAttributes>()
      .get()
      ->GetDeviceTypeForMetrics(base::BindOnce(
          &WebsiteMetricsServiceLacros::OnGetDeviceTypeForMetrics,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebsiteMetricsServiceLacros::Start() {
  // Check every `kFiveMinutes` to record websites usage time.
  five_minutes_timer_.Start(FROM_HERE, kFiveMinutes, this,
                            &WebsiteMetricsServiceLacros::CheckForFiveMinutes);

  // Check every `kNoisyAppKMReportInterval` to report noisy AppKM events.
  noisy_appkm_reporting_interval_timer_.Start(
      FROM_HERE, kNoisyAppKMReportInterval, this,
      &WebsiteMetricsServiceLacros::CheckForNoisyAppKMReportingInterval);
}

void WebsiteMetricsServiceLacros::SetWebsiteMetricsForTesting(
    std::unique_ptr<apps::WebsiteMetrics> website_metrics) {
  website_metrics_ = std::move(website_metrics);
}

void WebsiteMetricsServiceLacros::CheckForFiveMinutes() {
  if (website_metrics_) {
    website_metrics_->OnFiveMinutes();
  }
}

void WebsiteMetricsServiceLacros::CheckForNoisyAppKMReportingInterval() {
  if (website_metrics_) {
    website_metrics_->OnTwoHours();
  }
}

void WebsiteMetricsServiceLacros::OnGetDeviceTypeForMetrics(
    int user_type_by_device_type) {
  website_metrics_ = std::make_unique<apps::WebsiteMetrics>(
      profile_, user_type_by_device_type);
  Start();
}

}  // namespace apps

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/lacros_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace {

EnrollmentStatus GetEnrollmentStatus() {
  auto* service = chromeos::LacrosChromeServiceImpl::Get();
  switch (service->init_params()->device_mode) {
    case crosapi::mojom::DeviceMode::kUnknown:
      return EnrollmentStatus::kErrorGettingStatus;
    case crosapi::mojom::DeviceMode::kNotSet:
    case crosapi::mojom::DeviceMode::kConsumer:
    case crosapi::mojom::DeviceMode::kLegacyRetailMode:
    case crosapi::mojom::DeviceMode::kConsumerKioskAutolaunch:
      return EnrollmentStatus::kNonManaged;
    case crosapi::mojom::DeviceMode::kEnterprise:
    case crosapi::mojom::DeviceMode::kEnterpriseActiveDirectory:
    case crosapi::mojom::DeviceMode::kDemo:
      return EnrollmentStatus::kManaged;
  }
}

}  // namespace

LacrosMetricsProvider::LacrosMetricsProvider() = default;

LacrosMetricsProvider::~LacrosMetricsProvider() = default;

void LacrosMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  // Record whether the device we're running on is enterprise managed.
  UMA_STABILITY_HISTOGRAM_ENUMERATION(
      "UMA.EnrollmentStatus", GetEnrollmentStatus(),
      // static_cast because we only have macros for stability histograms.
      static_cast<int>(EnrollmentStatus::kMaxValue) + 1);
}

void LacrosMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProvideStabilityMetrics(uma_proto->mutable_system_profile());
  base::UmaHistogramBoolean("ChromeOS.IsLacrosBrowser", true);
}

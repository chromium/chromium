// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/lacros_metrics_provider.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/metrics.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

EnrollmentStatus GetEnrollmentStatus() {
  switch (chromeos::BrowserParamsProxy::Get()->DeviceMode()) {
    case crosapi::mojom::DeviceMode::kUnknown:
    case crosapi::mojom::DeviceMode::kEnterpriseActiveDirectoryDeprecated:
      return EnrollmentStatus::kErrorGettingStatus;
    case crosapi::mojom::DeviceMode::kNotSet:
    case crosapi::mojom::DeviceMode::kConsumer:
    case crosapi::mojom::DeviceMode::kLegacyRetailMode:
    case crosapi::mojom::DeviceMode::kConsumerKioskAutolaunch:
      return EnrollmentStatus::kNonManaged;
    case crosapi::mojom::DeviceMode::kEnterprise:
    case crosapi::mojom::DeviceMode::kDemo:
      return EnrollmentStatus::kManaged;
  }
}

}  // namespace

LacrosMetricsProvider::LacrosMetricsProvider() : weak_ptr_factory_(this) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::Metrics>()) {
    lacros_service->GetRemote<crosapi::mojom::Metrics>()->GetFullHardwareClass(
        base::BindOnce(&LacrosMetricsProvider::OnGetFullHardwareClass,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

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

void LacrosMetricsProvider::ProvideCurrentSessionUKMData() {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  EnrollmentStatus status = GetEnrollmentStatus();
  ukm::builders::ChromeOS_DeviceManagement(source_id)
      .SetEnrollmentStatus(static_cast<int64_t>(status))
      .Record(ukm::UkmRecorder::Get());
}

void LacrosMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* proto) {
  metrics::SystemProfileProto::Hardware* hardware = proto->mutable_hardware();
  hardware->set_full_hardware_class(full_hardware_class_);
}

void LacrosMetricsProvider::OnGetFullHardwareClass(
    const std::string& full_hardware_class) {
  full_hardware_class_ = full_hardware_class;
}

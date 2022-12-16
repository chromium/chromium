// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/low_disk_metrics_service.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace ash {

namespace {

KioskLowDiskSeverity GetSeverity(uint64_t free_disk_bytes) {
  if (free_disk_bytes < kLowDiskSevereThreshold) {
    return KioskLowDiskSeverity::kHigh;
  }
  if (free_disk_bytes < kLowDiskMediumThreshold) {
    return KioskLowDiskSeverity::kMedium;
  }
  return KioskLowDiskSeverity::kNone;
}

}  // namespace

const char kKioskSessionLowDiskSeverityHistogram[] =
    "Kiosk.Session.LowDiskSeverity";
const char kKioskSessionLowDiskHighestSeverityHistogram[] =
    "Kiosk.Session.LowDiskHighestSeverity";
const char kKioskLowDiskSeverity[] = "low-disk";

const uint64_t kLowDiskSevereThreshold = 512 << 20;  // 512MB
const uint64_t kLowDiskMediumThreshold = 1 << 30;    // 1GB

LowDiskMetricsService::LowDiskMetricsService()
    : LowDiskMetricsService(g_browser_process->local_state()) {}

// static
std::unique_ptr<LowDiskMetricsService> LowDiskMetricsService::CreateForTesting(
    PrefService* pref) {
  return base::WrapUnique(new LowDiskMetricsService(pref));
}

LowDiskMetricsService::LowDiskMetricsService(PrefService* prefs)
    : prefs_(prefs) {
  if (!UserDataAuthClient::Get()) {
    return;
  }
  UserDataAuthClient::Get()->AddObserver(this);
  ReportPreviousSessionLowDiskSeverity();
}

LowDiskMetricsService::~LowDiskMetricsService() {
  if (!UserDataAuthClient::Get()) {
    return;
  }
  UserDataAuthClient::Get()->RemoveObserver(this);
}

void LowDiskMetricsService::LowDiskSpace(
    const ::user_data_auth::LowDiskSpace& status) {
  auto severity = GetSeverity(status.disk_free_bytes());
  base::UmaHistogramEnumeration(kKioskSessionLowDiskSeverityHistogram,
                                severity);

  if (severity > low_disk_severity_) {
    low_disk_severity_ = severity;
    UpdateCurrentSessionLowDiskSeverity(severity);
  }
}

void LowDiskMetricsService::UpdateCurrentSessionLowDiskSeverity(
    KioskLowDiskSeverity severity) {
  prefs::ScopedDictionaryPrefUpdate update(prefs_, prefs::kKioskMetrics);
  update->SetInteger(kKioskLowDiskSeverity, static_cast<int>(severity));
}

void LowDiskMetricsService::ReportPreviousSessionLowDiskSeverity() {
  const auto& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);

  const auto* severity_value = metrics_dict.Find(kKioskLowDiskSeverity);
  if (!severity_value) {
    UpdateCurrentSessionLowDiskSeverity(low_disk_severity_);
    return;
  }

  auto severity = severity_value->GetIfInt();
  if (!severity.has_value()) {
    UpdateCurrentSessionLowDiskSeverity(low_disk_severity_);
    return;
  }
  base::UmaHistogramEnumeration(
      kKioskSessionLowDiskHighestSeverityHistogram,
      static_cast<KioskLowDiskSeverity>(severity.value()));
  UpdateCurrentSessionLowDiskSeverity(low_disk_severity_);
}

}  // namespace ash

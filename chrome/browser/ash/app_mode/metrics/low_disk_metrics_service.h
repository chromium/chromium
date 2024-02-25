// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_METRICS_LOW_DISK_METRICS_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_MODE_METRICS_LOW_DISK_METRICS_SERVICE_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"

namespace ash {

extern const char kKioskLowDiskSeverity[];
extern const char kKioskSessionLowDiskSeverityHistogram[];
extern const char kKioskSessionLowDiskHighestSeverityHistogram[];

extern const uint64_t kLowDiskMediumThreshold;
extern const uint64_t kLowDiskSevereThreshold;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with respective enum in tools/metrics/histograms/enums.xml
enum class KioskLowDiskSeverity {
  kNone = 0,
  kMedium = 1,
  kHigh = 2,
  kMaxValue = kHigh,
};

// LowDiskMetricsService tracks and reports severity of low disk notifications.
class LowDiskMetricsService : public UserDataAuthClient::Observer {
 public:
  LowDiskMetricsService();
  LowDiskMetricsService(LowDiskMetricsService&) = delete;
  LowDiskMetricsService& operator=(const LowDiskMetricsService&) = delete;
  ~LowDiskMetricsService() override;

  static std::unique_ptr<LowDiskMetricsService> CreateForTesting(
      PrefService* pref);

 private:
  explicit LowDiskMetricsService(PrefService* prefs);

  // Called when the device is running low on disk space.
  // This is responsible for tracking the severity metrics.
  void LowDiskSpace(const ::user_data_auth::LowDiskSpace& status) override;

  // Update a low disk severity for the current session.
  void UpdateCurrentSessionLowDiskSeverity(KioskLowDiskSeverity severity);

  // Report a highest severity of the previous session.
  void ReportPreviousSessionLowDiskSeverity();

  raw_ptr<PrefService> prefs_;
  // The highest low disk notification severity during the session.
  KioskLowDiskSeverity low_disk_severity_{KioskLowDiskSeverity::kNone};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_METRICS_LOW_DISK_METRICS_SERVICE_H_

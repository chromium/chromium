// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_DEVICE_METRICS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_DEVICE_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {
// A class for recording device metrics:
// - FamilyUser.NewUserAdded: Report NewUserAdded enum when a new user is added
// to the Chrome OS device. Recorded when user session starts.
// - FamilyUser.DeviceOwner: Boolean value indicates whether the primary
// user is the owner of device. Recorded at the beginning of the first active
// session daily.
// - FamilyUser.GaiaUsersCount: the number of Gaia users on the device.
// Recorded at the beginning of the first active session daily. The user count
// could be 0 if the managed device has ephemeral user policy.
// - FamilyUser.FamilyLinkUsersCount: the number of family link users on the
// device. Recorded at the beginning of the first active session daily.
class FamilyUserDeviceMetrics : public session_manager::SessionManagerObserver,
                                public FamilyUserMetricsService::Observer,
                                public DeviceSettingsService::Observer {
 public:
  // These enum values represent the new user type added to the device. These
  // values would be logged to UMA. Entries should not be renumbered and numeric
  // values should never be reused. Please keep in sync with "NewUserAdded" in
  // src/tools/metrics/histograms/enums.xml.
  enum class NewUserAdded {
    kOtherUserAdded = 0,
    kFamilyLinkUserAdded = 1,
    kRegularUserAdded = 2,
    kMaxValue = kRegularUserAdded
  };

  FamilyUserDeviceMetrics();
  FamilyUserDeviceMetrics(const FamilyUserDeviceMetrics&) = delete;
  FamilyUserDeviceMetrics& operator=(const FamilyUserDeviceMetrics&) = delete;
  ~FamilyUserDeviceMetrics() override;

  static const char* GetNewUserAddedHistogramNameForTest();
  static const char* GetDeviceOwnerHistogramNameForTest();
  static const char* GetFamilyLinkUsersCountHistogramNameForTest();
  static const char* GetGaiaUsersCountHistogramNameForTest();

  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  void ReportDeviceOwnership();

  const raw_ptr<const user_manager::UserManager> user_manager_;

  base::WeakPtrFactory<FamilyUserDeviceMetrics> weak_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_DEVICE_METRICS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USERTYPE_BY_DEVICETYPE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_USERTYPE_BY_DEVICETYPE_METRICS_PROVIDER_H_

#include <optional>

#include "base/feature_list.h"
#include "components/metrics/metrics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

class UserTypeByDeviceTypeMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver {
 public:
  // These enum values represent the type of user session for the primary
  // user. These values need to be kept in sync with MetricsLogSegment in
  // device_managerment_backend.proto (http://shortn/_PQKE7QSqoA)
  // These values are also used to calculate the values in UserDeviceMatrix
  // in src/tools/metrics/histograms/enums.xml so if a new value is added an
  // additional entry should be added there, and the browser test suite in
  // usertype_by_devicetype_metrics_provider_browsertest.cc
  // (http://shortn/_gD5uIM9Z78) should be updated to include the new
  // user / device type combo.
  enum class UserSegment {
    // Primary profile is for an unmanaged user.
    kUnmanaged = 0,
    // Primary profile is for a user belonging to a K-12 EDU organization.
    kK12 = 1,
    // Primary profile is for a user belonging to an university EDU
    // organization.
    kUniversity = 2,
    // Primary profile is for a user belonging to a non-profit organization.
    kNonProfit = 3,
    // Primary profile is for a user belonging to an enterprise organization.
    kEnterprise = 4,
    // Primary profile is for a demo session.
    // This value is not present in MetricsLogSegment and must not collide with
    // any values found there.
    kDemoMode = 65533,
    // Primary profile is for a kiosk app.
    // This value is not present in MetricsLogSegment and must not collide with
    // any values found there.
    kKioskApp = 65534,
    // Primary profile is for a managed guest session.
    // This value is not present in MetricsLogSegment and must not collide with
    // any values found there.
    kManagedGuestSession = 65535,
  };
  UserTypeByDeviceTypeMetricsProvider();
  UserTypeByDeviceTypeMetricsProvider(
      const UserTypeByDeviceTypeMetricsProvider&) = delete;
  UserTypeByDeviceTypeMetricsProvider& operator=(
      const UserTypeByDeviceTypeMetricsProvider&) = delete;
  ~UserTypeByDeviceTypeMetricsProvider() override;

  // MetricsProvider:
  bool ProvideHistograms() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // Returns user's segment for metrics logging.
  static UserSegment GetUserSegment(Profile* profile);

  static const char* GetHistogramNameForTesting();

  static int ConstructUmaValue(UserSegment user, policy::MarketSegment device);

 private:
  std::optional<UserSegment> user_segment_;
  std::optional<policy::MarketSegment> device_segment_;
};

#endif  // CHROME_BROWSER_METRICS_USERTYPE_BY_DEVICETYPE_METRICS_PROVIDER_H_

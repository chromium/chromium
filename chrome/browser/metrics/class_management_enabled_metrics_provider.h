// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CLASS_MANAGEMENT_ENABLED_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CLASS_MANAGEMENT_ENABLED_METRICS_PROVIDER_H_

#include <optional>

#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"

class ClassManagementEnabledMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver {
 public:
  enum class ClassManagementEnabled {
    // Class management disabled for the current user.
    kDisabled = 0,
    // Class management enabled for the current student user.
    kStudent = 1,
    // Class management enabled for the current teacher user.
    kTeacher = 2,
    kMaxValue = kTeacher,
  };

  ClassManagementEnabledMetricsProvider();
  ClassManagementEnabledMetricsProvider(
      const ClassManagementEnabledMetricsProvider&) = delete;
  ClassManagementEnabledMetricsProvider& operator=(
      const ClassManagementEnabledMetricsProvider&) = delete;
  ~ClassManagementEnabledMetricsProvider() override;

  // MetricsProvider:
  bool ProvideHistograms() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  std::optional<ClassManagementEnabled> segment_;
};

#endif  // CHROME_BROWSER_METRICS_CLASS_MANAGEMENT_ENABLED_METRICS_PROVIDER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/metrics_reporting_ash_test_helper.h"

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"

namespace crosapi {

// Create a noop delegate for ash metrics reporting in ash because the
// metrics service is not available under test.
class NoopMetricsDelegate : public crosapi::MetricsReportingAsh::Delegate {
  bool IsMetricsReportingEnabled() override { return false; }
  void SetMetricsReportingEnabled(bool) override {}
  std::string GetClientId() override { return ""; }
  base::CallbackListSubscription AddEnablementObserver(
      const base::RepeatingCallback<void(bool)>&) override {
    return base::CallbackListSubscription();
  }
};

std::unique_ptr<MetricsReportingAsh> CreateTestMetricsReportingAsh() {
  return std::make_unique<MetricsReportingAsh>(
      std::make_unique<NoopMetricsDelegate>());
}

}  // namespace crosapi

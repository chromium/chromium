// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"

#include <memory>

#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash_test_helper.h"
#include "components/metrics/metrics_service.h"

namespace crosapi {

std::unique_ptr<CrosapiManager> CreateCrosapiManagerWithTestRegistry() {
  TestCrosapiDependencyRegistry test_registry;
  return std::make_unique<CrosapiManager>(&test_registry);
}

TestCrosapiDependencyRegistry::TestCrosapiDependencyRegistry() = default;

TestCrosapiDependencyRegistry::~TestCrosapiDependencyRegistry() = default;

std::unique_ptr<MetricsReportingAsh>
TestCrosapiDependencyRegistry::CreateMetricsReportingAsh(
    metrics::MetricsService*) {
  return CreateTestMetricsReportingAsh();
}

}  // namespace crosapi

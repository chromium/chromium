// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_DEPENDENCY_REGISTRY_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_DEPENDENCY_REGISTRY_H_

#include <memory>

#include "chrome/browser/ash/crosapi/crosapi_dependency_registry.h"

namespace crosapi {

class CrosapiManager;

std::unique_ptr<CrosapiManager> CreateCrosapiManagerWithTestRegistry();

// Crosapi dependency registry that should only be used in test.
class TestCrosapiDependencyRegistry : public CrosapiDependencyRegistry {
 public:
  TestCrosapiDependencyRegistry();
  TestCrosapiDependencyRegistry(const TestCrosapiDependencyRegistry&) = delete;
  TestCrosapiDependencyRegistry& operator=(
      const TestCrosapiDependencyRegistry&) = delete;
  ~TestCrosapiDependencyRegistry() override;

  // This creates a MetricsReportingAsh object that does not use the metrics
  // service. It is safe to pass in a nullptr here.
  std::unique_ptr<MetricsReportingAsh> CreateMetricsReportingAsh(
      metrics::MetricsService* /* not_used */) override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_DEPENDENCY_REGISTRY_H_

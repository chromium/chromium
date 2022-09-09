// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_DEPENDENCY_REGISTRY_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_DEPENDENCY_REGISTRY_H_

#include <memory>

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace crosapi {

class MetricsReportingAsh;

// A registry which knows how to provide dependencies for crosapi. Because
// crosapi depends on many services, there may be instances where testing is
// difficult due to a lack of test doubles or implicit assumptions which do not
// hold true in test. This registry provides an indirection which allows users
// to swap out unfriendly dependencies.
class CrosapiDependencyRegistry {
 public:
  CrosapiDependencyRegistry() = default;
  CrosapiDependencyRegistry(const CrosapiDependencyRegistry&) = delete;
  CrosapiDependencyRegistry& operator=(const CrosapiDependencyRegistry&) =
      delete;
  virtual ~CrosapiDependencyRegistry() = default;

  virtual std::unique_ptr<MetricsReportingAsh> CreateMetricsReportingAsh(
      metrics::MetricsService* metrics_service);
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_DEPENDENCY_REGISTRY_H_

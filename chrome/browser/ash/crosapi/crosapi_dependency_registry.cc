// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_dependency_registry.h"

#include <memory>

#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"

namespace crosapi {

std::unique_ptr<MetricsReportingAsh>
crosapi::CrosapiDependencyRegistry::CreateMetricsReportingAsh(
    metrics::MetricsService* metrics_service) {
  return MetricsReportingAsh::CreateMetricsReportingAsh(metrics_service);
}

}  // namespace crosapi

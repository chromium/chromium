// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/feature_discovery_duration_reporter.h"

#include "base/check_op.h"

namespace ash {

namespace {
FeatureDiscoveryDurationReporter* g_reporter_instance = nullptr;
}  // namespace

FeatureDiscoveryDurationReporter::FeatureDiscoveryDurationReporter() {
  DCHECK(!g_reporter_instance);
  g_reporter_instance = this;
}

FeatureDiscoveryDurationReporter::~FeatureDiscoveryDurationReporter() {
  DCHECK_EQ(this, g_reporter_instance);
  g_reporter_instance = nullptr;
}

// static
FeatureDiscoveryDurationReporter*
FeatureDiscoveryDurationReporter::GetInstance() {
  DCHECK(g_reporter_instance);
  return g_reporter_instance;
}

}  // namespace ash

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/tether/mock_host_connection_metrics_logger.h"

#include "ash/components/tether/active_host.h"

namespace ash {

namespace tether {

MockHostConnectionMetricsLogger::MockHostConnectionMetricsLogger(
    ActiveHost* active_host)
    : HostConnectionMetricsLogger(active_host) {}

MockHostConnectionMetricsLogger::~MockHostConnectionMetricsLogger() = default;

}  // namespace tether

}  // namespace ash

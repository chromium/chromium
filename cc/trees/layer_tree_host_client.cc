// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_client.h"

#include "cc/metrics/web_vital_metrics.h"

namespace cc {

std::unique_ptr<WebVitalMetrics> LayerTreeHostClient::GetWebVitalMetrics() {
  return nullptr;
}

}  // namespace cc

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/histogram_scope.h"

namespace base::trace_event {
namespace {

thread_local uint64_t g_histogram_flow_id = 0;

}

HistogramScope::HistogramScope(uint64_t flow_id) : flow_id_(flow_id) {
  DCHECK_EQ(g_histogram_flow_id, 0U);
  g_histogram_flow_id = flow_id_;
}

HistogramScope::~HistogramScope() {
  DCHECK_EQ(g_histogram_flow_id, flow_id_);
  g_histogram_flow_id = 0;
}

std::optional<uint64_t> HistogramScope::GetFlowId() {
  if (g_histogram_flow_id == 0) {
    return std::nullopt;
  }
  return g_histogram_flow_id;
}

}  // namespace base::trace_event

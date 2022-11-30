// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/custom_metrics_recorder.h"

#include "base/check_op.h"

namespace cc {
namespace {

CustomMetricRecorder* g_instance = nullptr;

}  // namespace

// static
CustomMetricRecorder* CustomMetricRecorder::Get() {
  return g_instance;
}

CustomMetricRecorder::CustomMetricRecorder() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

CustomMetricRecorder::~CustomMetricRecorder() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace cc

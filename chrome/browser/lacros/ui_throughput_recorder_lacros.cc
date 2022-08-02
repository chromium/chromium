// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/ui_throughput_recorder_lacros.h"

#include "base/metrics/histogram_macros.h"

UiThroughputRecorderLacros::UiThroughputRecorderLacros() = default;
UiThroughputRecorderLacros::~UiThroughputRecorderLacros() = default;

void UiThroughputRecorderLacros::ReportPercentDroppedFramesInOneSecoundWindow(
    double percentage) {
  UMA_HISTOGRAM_PERCENTAGE(
      "Chrome.Lacros.Smoothness.PercentDroppedFrames_1sWindow", percentage);
}

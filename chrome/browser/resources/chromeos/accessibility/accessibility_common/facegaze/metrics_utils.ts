// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

/** A class used to record metrics for FaceGaze. */
export class MetricsUtils {
  private latencies_: number[] = [];

  addFaceLandmarkerResultLatency(val: number): void {
    this.latencies_.push(val);
    if (this.latencies_.length >= 100) {
      this.writeHistogram_();
      this.latencies_ = [];
    }
  }

  private writeHistogram_(): void {
    let sum = 0;
    for (const val of this.latencies_) {
      sum += val;
    }

    const average = Math.ceil(sum / this.latencies_.length);
    chrome.metricsPrivate.recordMediumTime(
        MetricsUtils.FACELANDMARKER_PERFORMANCE_METRIC, average);
  }
}

export namespace MetricsUtils {
  /**
   * The metric used to record the average FaceLandmarker performance time on a
   * single video frame (in milliseconds).
   */
  export const FACELANDMARKER_PERFORMANCE_METRIC =
      'Accessibility.FaceGaze.AverageFaceLandmarkerLatency';
}

TestImportManager.exportForTesting(MetricsUtils);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['facegaze_test_base.js']);

FaceGazeMetricsUtilsTest = class extends FaceGazeTestBase {
  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ true);
  }
};

AX_TEST_F('FaceGazeMetricsUtilsTest', 'FaceLandmarkerLatency', function() {
  const utils = this.getFaceGaze().metricsUtils_;
  assertEquals(0, utils.latencies_.length);

  let histogramCount = 0;
  let metricName = '';
  let metricValue = 0;
  chrome.metricsPrivate.recordMediumTime = (name, value) => {
    ++histogramCount;
    metricName = name;
    metricValue = value;
  };

  for (let i = 1; i <= 3; ++i) {
    for (let j = 0; j < 100; ++j) {
      utils.addFaceLandmarkerResultLatency(j);
    }

    // Ensure that the histogram is recorded after 100 calls and that the
    // internal array gets reset.
    assertEquals(i, histogramCount);
    assertEquals(
        'Accessibility.FaceGaze.AverageFaceLandmarkerLatency', metricName);
    assertEquals(50, metricValue);
    assertEquals(0, utils.latencies_.length);
  }
});

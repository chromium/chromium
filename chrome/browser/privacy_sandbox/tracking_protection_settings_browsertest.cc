// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class TrackingProtectionSettingsMetricsBrowserTest
    : public InProcessBrowserTest {
 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(TrackingProtectionSettingsMetricsBrowserTest,
                       RecordsMetricsOnStartup) {
  histogram_tester_.ExpectUniqueSample("Settings.TrackingProtection.Enabled",
                                       false, 1);
}

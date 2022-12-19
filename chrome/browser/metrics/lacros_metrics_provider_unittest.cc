// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/lacros_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

using UkmEntry = ukm::builders::ChromeOS_DeviceManagement;

TEST(LacrosMetricsProviderTest, EnrollmentStatusRecordedForCurrentSession) {
  base::test::TaskEnvironment task_environment;
  chromeos::ScopedLacrosServiceTestHelper lacros_test_helper;

  // Simulate lacros initialization on an enterprise-enrolled device.
  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->device_mode = crosapi::mojom::DeviceMode::kEnterprise;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  // Provide current session metrics.
  base::HistogramTester histogram_tester;
  LacrosMetricsProvider metrics_provider;
  metrics::ChromeUserMetricsExtension uma_proto;
  metrics_provider.ProvideCurrentSessionData(&uma_proto);

  // Enrollment status is recorded.
  histogram_tester.ExpectUniqueSample(
      "UMA.EnrollmentStatus", static_cast<int>(EnrollmentStatus::kManaged), 1);
}

TEST(LacrosMetricsProviderTest,
     EnrollmentStatusRecordedForCurrentSessionUKMData) {
  base::test::TaskEnvironment task_environment;
  chromeos::ScopedLacrosServiceTestHelper lacros_test_helper;

  // Simulate lacros initialization on an enterprise-enrolled device.
  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->device_mode = crosapi::mojom::DeviceMode::kEnterprise;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  // Provide current session UKM Data.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  LacrosMetricsProvider metrics_provider;
  metrics_provider.ProvideCurrentSessionUKMData();

  // Enrollment status is recorded as a UKM entry
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], UkmEntry::kEnrollmentStatusName,
      static_cast<int>(EnrollmentStatus::kManaged));
}

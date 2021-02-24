// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/lacros_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

TEST(LacrosMetricsProviderTest, EnrollmentStatusRecordedForCurrentSession) {
  base::test::TaskEnvironment task_environment;

  // Simulate lacros initialization on an enterprise-enrolled device.
  chromeos::LacrosChromeServiceImpl lacros_chrome_service(/*delegate=*/nullptr);
  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->device_mode = crosapi::mojom::DeviceMode::kEnterprise;
  lacros_chrome_service.SetInitParamsForTests(std::move(init_params));

  // Provide current session metrics.
  base::HistogramTester histogram_tester;
  LacrosMetricsProvider metrics_provider;
  metrics::ChromeUserMetricsExtension uma_proto;
  metrics_provider.ProvideCurrentSessionData(&uma_proto);

  // Enrollment status is recorded.
  histogram_tester.ExpectUniqueSample(
      "UMA.EnrollmentStatus", static_cast<int>(EnrollmentStatus::kManaged), 1);
}

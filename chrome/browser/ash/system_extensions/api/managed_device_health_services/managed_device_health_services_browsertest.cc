// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_extensions/api/test_support/system_extensions_api_browsertest.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

static constexpr const char kTestsDir[] =
    "chrome/browser/ash/system_extensions/api/managed_device_health_services/"
    "test";

static constexpr const char kManifestTemplate[] = R"(
{
  "name": "Test Telemetry API",
  "short_name": "Test",
  "service_worker_url": "/%s",
  "id": "01020304",
  "type": "managed-device-health-services"
})";

}  // namespace

class ManagedDeviceHealthServicesBrowserTest
    : public SystemExtensionsApiBrowserTest {
 public:
  ManagedDeviceHealthServicesBrowserTest()
      : SystemExtensionsApiBrowserTest({
            .tests_dir = kTestsDir,
            .manifest_template = kManifestTemplate,
            .additional_src_files = {},
            .additional_gen_files = {},
        }) {
    features_list_.InitAndEnableFeature(
        features::kSystemExtensionsManagedDeviceHealthServices);
  }

 private:
  base::test::ScopedFeatureList features_list_;
};

IN_PROC_BROWSER_TEST_F(ManagedDeviceHealthServicesBrowserTest, SmokeTest) {
  RunTest("smoke_test.js");
}

}  // namespace ash

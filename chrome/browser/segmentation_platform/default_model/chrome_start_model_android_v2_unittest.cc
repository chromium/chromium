// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android_v2.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class ChromeStartModelV2Test : public DefaultModelTestBase {
 public:
  ChromeStartModelV2Test()
      : DefaultModelTestBase(std::make_unique<ChromeStartModelV2>()) {}
  ~ChromeStartModelV2Test() override = default;
};

TEST_F(ChromeStartModelV2Test, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ChromeStartModelV2Test, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
  // 3 input features defined in `kChromeStartUMAFeatures`, set all to 0.
  ModelProvider::Request input = {0, 0, 0};
  const float kDefaultReturnTimeSeconds = 28800;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           {kDefaultReturnTimeSeconds});
  ExpectClassifierResults(input, {kChromeStartAndroidV2Label8HourInMs});

  // Set to higher values, the model returns the same result.
  input = {3, 6, 3};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           {kDefaultReturnTimeSeconds});
  ExpectClassifierResults(input, {kChromeStartAndroidV2Label8HourInMs});
}

}  // namespace segmentation_platform

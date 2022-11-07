// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class ChromeStartModelTest : public DefaultModelTestBase {
 public:
  ChromeStartModelTest()
      : DefaultModelTestBase(std::make_unique<ChromeStartModel>()) {}
  ~ChromeStartModelTest() override = default;
};

TEST_F(ChromeStartModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ChromeStartModelTest, ExecuteModelWithInput) {
  const unsigned kMvIndex = 3;

  ModelProvider::Request input = {0.3, 1.4, -2, 4, 1, 6, 7, 8};

  input[kMvIndex] = 0;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0});

  input[kMvIndex] = -3;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0});

  input[kMvIndex] = 1;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1});

  input[kMvIndex] = 4.6;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{1});

  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/true,
                           /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{1, 2, 3, 4, 5, 6, 7, 8, 9},
                           /*expected_error=*/true, /*expected_result=*/{0});
}

}  // namespace segmentation_platform

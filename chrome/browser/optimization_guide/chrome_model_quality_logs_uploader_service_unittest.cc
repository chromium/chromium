// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_model_quality_logs_uploader_service.h"

#include <memory>

#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/service/test_variations_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using variations::TestVariationsService;

class ChromeModelQualityLogsUploaderServiceTest : public testing::Test {
 public:
  ChromeModelQualityLogsUploaderServiceTest() {
    TestVariationsService::RegisterPrefs(pref_service_.registry());
    variations_service_ =
        std::make_unique<TestVariationsService>(&pref_service_);
  }
  ~ChromeModelQualityLogsUploaderServiceTest() override {
    // Clear out any previous references to avoid complaints from the memory
    // management system about live references to freed memory.
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
  }

  ChromeModelQualityLogsUploaderService MakeUploaderService() {
    return ChromeModelQualityLogsUploaderService(
        nullptr, &pref_service_,
        base::WeakPtr<ModelExecutionFeaturesController>());
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestVariationsService> variations_service_;
};

TEST_F(ChromeModelQualityLogsUploaderServiceTest,
       SetSystemMetadata_DogfoodStatus_UnsetWhenUnknown) {
  TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);

  ChromeModelQualityLogsUploaderService service = MakeUploaderService();
  proto::LoggingMetadata metadata;
  service.SetSystemMetadata(&metadata);
  EXPECT_FALSE(metadata.is_likely_dogfood_client());
}

TEST_F(ChromeModelQualityLogsUploaderServiceTest,
       SetSystemMetadata_DogfoodStatus_FalseForNonDogfoodClient) {
  variations_service_->SetIsLikelyDogfoodClientForTesting(false);
  TestingBrowserProcess::GetGlobal()->SetVariationsService(
      variations_service_.get());

  ChromeModelQualityLogsUploaderService service = MakeUploaderService();
  proto::LoggingMetadata metadata;
  service.SetSystemMetadata(&metadata);
  EXPECT_FALSE(metadata.is_likely_dogfood_client());
}

TEST_F(ChromeModelQualityLogsUploaderServiceTest,
       SetSystemMetadata_DogfoodStatus_TrueForDogfoodClient) {
  variations_service_->SetIsLikelyDogfoodClientForTesting(true);
  TestingBrowserProcess::GetGlobal()->SetVariationsService(
      variations_service_.get());

  ChromeModelQualityLogsUploaderService service = MakeUploaderService();
  proto::LoggingMetadata metadata;
  service.SetSystemMetadata(&metadata);
  EXPECT_TRUE(metadata.is_likely_dogfood_client());
}

TEST_F(ChromeModelQualityLogsUploaderServiceTest,
       SetSystemMetadata_ClientIdsAreStripped) {
  TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);

  ChromeModelQualityLogsUploaderService service = MakeUploaderService();
  proto::LoggingMetadata metadata;
  metadata.mutable_system_profile()->set_client_uuid("123");
  metadata.mutable_system_profile()->mutable_cloned_install_info()
      ->set_cloned_from_client_id(123);
  service.SetSystemMetadata(&metadata);
  EXPECT_FALSE(metadata.system_profile().has_client_uuid());
  EXPECT_FALSE(metadata.system_profile().cloned_install_info().has_cloned_from_client_id());
}


}  // namespace optimization_guide

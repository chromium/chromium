// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/manatee/manatee_cache.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {
constexpr char kEmail[] = "test-user@example.com";
constexpr char16_t kEmail16[] = u"test-user@example.com";
constexpr char kRequestUrl[] = "http://example/url";
}  // namespace

class ManateeCacheTest : public testing::Test {
 public:
  ManateeCacheTest() = default;
  ~ManateeCacheTest() override = default;

  void OnResponseCallback(EmbeddingsList& reply) { reply_ = reply; }

  EmbeddingsList& GrabReply() { return reply_; }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &url_loader_factory_));
    profile_ =
        profile_manager_->CreateTestingProfile(kEmail, /*prefs=*/{}, kEmail16,
                                               /*avatar_id=*/0, factories);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env_ = identity_test_env_adaptor_.get()->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&url_loader_factory_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  EmbeddingsList reply_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  base::WeakPtrFactory<ManateeCacheTest> weak_factory_{this};
};

TEST_F(ManateeCacheTest, URLLoaderSingleInputString) {
  std::unique_ptr<ManateeCache> manateeCache =
      std::make_unique<ManateeCache>(profile_, shared_url_loader_factory_);
  EmbeddingsList expected = {{0.1, 0.2, 0.3}};
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  constexpr char kValidJsonResponse[] = R"(
      {
        "embedding": [[0.1, 0.2, 0.3]]
      })";
  url_loader_factory_.AddResponse(kRequestUrl, kValidJsonResponse,
                                  net::HTTP_OK);

  manateeCache->RegisterCallback(base::BindOnce(
      &ManateeCacheTest::OnResponseCallback, weak_factory_.GetWeakPtr()));

  manateeCache->UrlLoader({"Hello World!"});
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ManateeCacheTest::GrabReply(), expected);
}

TEST_F(ManateeCacheTest, URLLoaderMultiInputString) {
  std::unique_ptr<ManateeCache> manateeCache =
      std::make_unique<ManateeCache>(profile_, shared_url_loader_factory_);
  EmbeddingsList expected = {{0.1, 0.2, 0.3}, {0.4, 0.5, 0.6}};
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  constexpr char kValidJsonResponse[] = R"(
      {
        "embedding": [[0.1, 0.2, 0.3], [0.4, 0.5, 0.6]]
      })";
  url_loader_factory_.AddResponse(kRequestUrl, kValidJsonResponse,
                                  net::HTTP_OK);

  manateeCache->RegisterCallback(base::BindOnce(
      &ManateeCacheTest::OnResponseCallback, weak_factory_.GetWeakPtr()));

  manateeCache->UrlLoader({"Hello World!", "Hello"});
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ManateeCacheTest::GrabReply(), expected);
}

TEST_F(ManateeCacheTest, StringFormatting) {
  std::unique_ptr<ManateeCache> manateeCache =
      std::make_unique<ManateeCache>(profile_, shared_url_loader_factory_);
  std::string expected =
      "{\n        \"text\": [\"Hello World!\", \"Hi.\"]\n      }";
  std::string response = manateeCache->GetRequestBody(
      manateeCache->VectorToString({"Hello World!", "Hi."}));
  EXPECT_EQ(response, expected);
}

}  // namespace app_list

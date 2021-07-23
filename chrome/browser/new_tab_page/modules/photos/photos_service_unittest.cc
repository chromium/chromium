// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "base/hash/hash.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

class PhotosServiceTest : public testing::Test {
 public:
  PhotosServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();
    service_ = std::make_unique<PhotosService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env.identity_manager());
    identity_test_env.MakePrimaryAccountAvailable("example@google.com",
                                                  signin::ConsentLevel::kSync);
  }

  void TearDown() override {
    service_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<PhotosService> service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  signin::IdentityTestEnvironment identity_test_env;
};

TEST_F(PhotosServiceTest, PassesDataOnSuccess) {
  std::vector<photos::mojom::MemoryPtr> actual_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            actual_memories = std::move(memories);
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(
      test_url_loader_factory_.pending_requests()
          ->at(0)
          .request.headers.ToString(),
      "Content-Type: application/json\r\nAuthorization: Bearer foo\r\n\r\n");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/chrome_ntp/"
      "read_reminiscing_content",
      R"(
        {
          "bundle": [
            {
              "bundleKey": "key1",
              "title": {
                "header": "Title 1",
                "subheader": "Something something 1"
              }
            },
            {
              "bundleKey": "key2",
              "title": {
                "header": "Title 2",
                "subheader": "Something something 2"
              }
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);

  EXPECT_EQ(2u, actual_memories.size());
  EXPECT_EQ("Title 1", actual_memories.at(0)->title);
  EXPECT_EQ("key1", actual_memories.at(0)->id);
  EXPECT_EQ("Title 2", actual_memories.at(1)->title);
  EXPECT_EQ("key2", actual_memories.at(1)->id);
}

TEST_F(PhotosServiceTest, PassesNoDataOnAuthError) {
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&empty_response](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceTest, PassesNoDataOnNetError) {
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&empty_response](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());

  EXPECT_EQ(
      test_url_loader_factory_.pending_requests()
          ->at(0)
          .request.headers.ToString(),
      "Content-Type: application/json\r\nAuthorization: Bearer foo\r\n\r\n");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/chrome_ntp/"
      "read_reminiscing_content",
      std::string(), net::HTTP_BAD_REQUEST,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceTest, PassesNoDataOnEmptyResponse) {
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&empty_response](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/chrome_ntp/"
      "read_reminiscing_content",
      "", net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceTest, PassesNoDataOnMissingItemKey) {
  std::vector<photos::mojom::MemoryPtr> actual_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            actual_memories = std::move(memories);
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/chrome_ntp/"
      "read_reminiscing_content",
      R"(
        {
          "bundle": [],
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);

  EXPECT_TRUE(actual_memories.empty());
}

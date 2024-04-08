// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"

#include "base/barrier_closure.h"
#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

class DriveServiceTest : public testing::Test {
 public:
  DriveServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();
    service_ = std::make_unique<DriveService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env.identity_manager(),
        &mock_segmentation_platform_service_, "en-US", &prefs_);
    identity_test_env.MakePrimaryAccountAvailable("example@google.com",
                                                  signin::ConsentLevel::kSync);
    service_->RegisterProfilePrefs(prefs_.registry());
  }

  void TearDown() override {
    service_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DriveService> service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  signin::IdentityTestEnvironment identity_test_env;
  segmentation_platform::MockSegmentationPlatformService
      mock_segmentation_platform_service_;
  TestingPrefServiceSimple prefs_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DriveServiceTest, PassesDataOnSuccess) {
  std::vector<file_suggestion::mojom::FilePtr> actual_documents;
  auto quit_closure = task_environment_.QuitClosure();
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            actual_documents = std::move(documents);
            quit_closure.Run();
          }));

  // Make sure we are not in the dismissed time window.
  prefs_.SetTime(DriveService::kLastDismissedTimePrefName, base::Time::Now());
  task_environment_.AdvanceClock(DriveService::kDismissDuration);

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  std::string request_body(test_url_loader_factory_.pending_requests()
                               ->at(0)
                               .request.request_body->elements()
                               ->at(0)
                               .As<network::DataElementBytes>()
                               .AsStringPiece());
  auto body_value = base::JSONReader::Read(request_body);
  EXPECT_EQ("en-US", *body_value->GetDict().FindStringByDottedPath(
                         "client_info.language_code"));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items",
      R"(
        {
          "item": [
            {
              "itemId":"234",
              "url":"https://google.com/foo",
              "driveItem": {
                "title": "Foo foo",
                "mimeType": "application/vnd.google-apps.spreadsheet"
              },
              "justification": {
                "unstructuredJustificationDescription": {
                  "textSegment": [
                    {
                      "text": "Foo foo"
                    }
                  ]
                }
              }
            },
            {
              "itemId":"123",
              "url":"https://google.com/bar",
              "driveItem": {
                "title": "Bar",
                "mimeType": "application/vnd.google-apps.document"
              },
              "justification": {
                "unstructuredJustificationDescription": {
                  "textSegment": [
                    {
                      "text": "Foo "
                    },
                    {
                      "text": "bar foo bar"
                    }
                  ]
                }
              }
            },
            {
              "driveItem": {
              }
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(2u, actual_documents.size());
  EXPECT_EQ("Foo foo", actual_documents.at(0)->title);
  EXPECT_EQ("application/vnd.google-apps.spreadsheet",
            actual_documents.at(0)->mime_type);
  EXPECT_EQ("Foo foo", actual_documents.at(0)->justification_text);
  EXPECT_EQ("https://google.com/foo", actual_documents.at(0)->item_url.spec());
  EXPECT_EQ("Bar", actual_documents.at(1)->title);
  EXPECT_EQ("123", actual_documents.at(1)->id);
  EXPECT_EQ("application/vnd.google-apps.document",
            actual_documents.at(1)->mime_type);
  EXPECT_EQ("Foo bar foo bar", actual_documents.at(1)->justification_text);
  EXPECT_EQ("https://google.com/bar", actual_documents.at(1)->item_url.spec());
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
  // The third item is malformed. So, even though we can display the first two
  // items, we report a content error.
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Drive.ItemSuggestRequestResult",
                   ItemSuggestRequestResult::kContentError));
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Drive.FileCount", 2));
}

TEST_F(DriveServiceTest, PassesDataToMultipleRequestsToDriveService) {
  auto quit_closure = task_environment_.QuitClosure();
  auto barrier_closure = base::BarrierClosure(4, quit_closure);

  std::vector<file_suggestion::mojom::FilePtr> response1;
  std::vector<file_suggestion::mojom::FilePtr> response2;
  std::vector<file_suggestion::mojom::FilePtr> response3;
  std::vector<file_suggestion::mojom::FilePtr> response4;

  base::MockCallback<DriveService::GetFilesCallback> callback1;
  base::MockCallback<DriveService::GetFilesCallback> callback2;
  base::MockCallback<DriveService::GetFilesCallback> callback3;
  base::MockCallback<DriveService::GetFilesCallback> callback4;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            response1 = std::move(documents);
            barrier_closure.Run();
          }));
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            response2 = std::move(documents);
            barrier_closure.Run();
          }));
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            response3 = std::move(documents);
            barrier_closure.Run();
          }));
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            response4 = std::move(documents);
            barrier_closure.Run();
          }));
  service_->GetDriveFiles(callback1.Get());
  service_->GetDriveFiles(callback2.Get());
  service_->GetDriveFiles(callback3.Get());
  service_->GetDriveFiles(callback4.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items",
      R"(
        {
          "item": [
            {
              "itemId":"234",
              "url": "https://google.com/foo",
              "driveItem": {
                "title": "Foo foo",
                "mimeType": "application/vnd.google-apps.spreadsheet"
              },
              "justification": {
                "unstructuredJustificationDescription": {
                  "textSegment": [
                    {
                      "text": "Foo foo"
                    }
                  ]
                }
              }
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(1u, response1.size());
  EXPECT_EQ(1u, response2.size());
  EXPECT_EQ(1u, response3.size());
  EXPECT_EQ(1u, response4.size());
  EXPECT_EQ("Foo foo", response1.at(0)->title);
  EXPECT_EQ("application/vnd.google-apps.spreadsheet",
            response1.at(0)->mime_type);
  EXPECT_EQ("Foo foo", response1.at(0)->justification_text);
  EXPECT_EQ("234", response1.at(0)->id);
  EXPECT_EQ("Foo foo", response2.at(0)->title);
  EXPECT_EQ("application/vnd.google-apps.spreadsheet",
            response2.at(0)->mime_type);
  EXPECT_EQ("Foo foo", response2.at(0)->justification_text);
  EXPECT_EQ("234", response2.at(0)->id);
  EXPECT_EQ("Foo foo", response3.at(0)->title);
  EXPECT_EQ("application/vnd.google-apps.spreadsheet",
            response3.at(0)->mime_type);
  EXPECT_EQ("Foo foo", response3.at(0)->justification_text);
  EXPECT_EQ("234", response3.at(0)->id);
  EXPECT_EQ("Foo foo", response4.at(0)->title);
  EXPECT_EQ("application/vnd.google-apps.spreadsheet",
            response4.at(0)->mime_type);
  EXPECT_EQ("Foo foo", response4.at(0)->justification_text);
  EXPECT_EQ("234", response4.at(0)->id);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Drive.ItemSuggestRequestResult",
                   ItemSuggestRequestResult::kSuccess));
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Drive.FileCount", 1));
}

TEST_F(DriveServiceTest, PassesCachedDataIfRequested) {
  constexpr char kDriveData[] = R"(
        {
          "item": [
            {
              "itemId":"234",
              "url":"https://google.com/foo",
              "driveItem": {
                "title": "Foo foo",
                "mimeType": "application/vnd.google-apps.spreadsheet"
              },
              "justification": {
                "unstructuredJustificationDescription": {
                  "textSegment": [
                    {
                      "text": "Foo foo"
                    }
                  ]
                }
              }
            }
          ]
        }
      )";
  std::vector<file_suggestion::mojom::FilePtr> response;
  base::MockCallback<DriveService::GetFilesCallback> callback;

  auto quit_closure = task_environment_.QuitClosure();
  EXPECT_CALL(callback, Run(testing::_))
      .WillRepeatedly(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            response = std::move(documents);
            quit_closure.Run();
          }));

  // Enable caching.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpDriveModule,
      {{ntp_features::kNtpDriveModuleCacheMaxAgeSParam, "10"}});

  // Make sure we are not in the dismissed time window.
  prefs_.SetTime(DriveService::kLastDismissedTimePrefName, base::Time::Now());
  task_environment_.AdvanceClock(DriveService::kDismissDuration);

  // First request populates the cache.
  service_->GetDriveFiles(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo_token", base::Time());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", kDriveData,
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_FALSE(response.empty());
  EXPECT_EQ("234", response[0]->id);
  EXPECT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));

  // Subsequent fetch should use cache.
  response.clear();
  service_->GetDriveFiles(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo_token", base::Time());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(response.empty());
  EXPECT_EQ("234", response[0]->id);
  EXPECT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));

  // Should re-request if cache expires.
  quit_closure = task_environment_.QuitClosure();
  response.clear();
  task_environment_.AdvanceClock(base::Seconds(11));
  service_->GetDriveFiles(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo_token", base::Time());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", kDriveData,
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ("234", response[0]->id);
  EXPECT_EQ(2,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));

  // Should re-request if token changes.
  quit_closure = task_environment_.QuitClosure();
  response.clear();
  service_->GetDriveFiles(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "bar_token", base::Time());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", kDriveData,
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();
  EXPECT_EQ("234", response[0]->id);
  EXPECT_EQ(3,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
}

TEST_F(DriveServiceTest, PassesDataIfSegmentationIsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(ntp_features::kNtpDriveModuleSegmentation);

  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);

  EXPECT_CALL(
      mock_segmentation_platform_service_,
      GetClassificationResult(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(result));

  std::vector<file_suggestion::mojom::FilePtr> actual_documents;
  auto quit_closure = task_environment_.QuitClosure();
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            actual_documents = std::move(documents);
            quit_closure.Run();
          }));

  // Make sure we are not in the dismissed time window.
  prefs_.SetTime(DriveService::kLastDismissedTimePrefName, base::Time::Now());
  task_environment_.AdvanceClock(DriveService::kDismissDuration);

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  std::string request_body(test_url_loader_factory_.pending_requests()
                               ->at(0)
                               .request.request_body->elements()
                               ->at(0)
                               .As<network::DataElementBytes>()
                               .AsStringPiece());
  auto body_value = base::JSONReader::Read(request_body);
  EXPECT_EQ("en-US", *body_value->GetDict().FindStringByDottedPath(
                         "client_info.language_code"));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items",
      R"(
        {
          "item": [
            {
              "itemId":"234",
              "url": "https://google.com/foo",
              "driveItem": {
                "title": "Foo foo",
                "mimeType": "application/vnd.google-apps.spreadsheet"
              },
              "justification": {
                "unstructuredJustificationDescription": {
                  "textSegment": [
                    {
                      "text": "Foo foo"
                    }
                  ]
                }
              }
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(1u, actual_documents.size());
}

TEST_F(DriveServiceTest, AddsClientTagIfRequested) {
  // Make sure we are not in the dismissed time window.
  prefs_.SetTime(DriveService::kLastDismissedTimePrefName, base::Time::Now());
  task_environment_.AdvanceClock(DriveService::kDismissDuration);

  // Set client tag.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpDriveModule,
      {{ntp_features::kNtpDriveModuleExperimentGroupParam, "foo"}});

  service_->GetDriveFiles(DriveService::GetFilesCallback());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo_token", base::Time());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  std::string request_body(test_url_loader_factory_.pending_requests()
                               ->at(0)
                               .request.request_body->elements()
                               ->at(0)
                               .As<network::DataElementBytes>()
                               .AsStringPiece());
  auto body_value = base::JSONReader::Read(request_body);
  EXPECT_EQ("foo", *body_value->GetDict().FindStringByDottedPath(
                       "client_info.client_tags.name"));
}

TEST_F(DriveServiceTest, PassesNoDataIfDismissed) {
  bool passed_no_data = false;
  base::MockCallback<DriveService::GetFilesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&passed_no_data](std::vector<file_suggestion::mojom::FilePtr> suggestions) {
            passed_no_data = suggestions.empty();
          }));

  prefs_.SetTime(DriveService::kLastDismissedTimePrefName, base::Time::Now());
  service_->GetDriveFiles(callback.Get());

  EXPECT_TRUE(passed_no_data);
}

TEST_F(DriveServiceTest, PassesNoDataOnAuthError) {
  bool token_is_valid = true;

  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&token_is_valid](std::vector<file_suggestion::mojom::FilePtr> suggestions) {
            token_is_valid = !suggestions.empty();
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  EXPECT_FALSE(token_is_valid);
  ASSERT_EQ(0,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
}

TEST_F(DriveServiceTest, PassesNoDataOnNetError) {
  bool empty_response = false;
  auto quit_closure = task_environment_.QuitClosure();
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> suggestions) {
            empty_response = suggestions.empty();
            quit_closure.Run();
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());

  EXPECT_EQ(
      test_url_loader_factory_.pending_requests()
          ->at(0)
          .request.headers.ToString(),
      "Content-Type: application/json\r\nAuthorization: Bearer foo\r\n\r\n");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", std::string(),
      net::HTTP_BAD_REQUEST,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(empty_response);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Drive.ItemSuggestRequestResult",
                   ItemSuggestRequestResult::kNetworkError));
}

TEST_F(DriveServiceTest, PassesNoDataOnEmptyResponse) {
  bool empty_response = false;
  auto quit_closure = task_environment_.QuitClosure();

  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> suggestions) {
            empty_response = suggestions.empty();
            quit_closure.Run();
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", "", net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(empty_response);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Drive.ItemSuggestRequestResult",
                   ItemSuggestRequestResult::kJsonParseError));
}

TEST_F(DriveServiceTest, PassesNoDataOnMissingItemKey) {
  auto quit_closure = task_environment_.QuitClosure();
  std::vector<file_suggestion::mojom::FilePtr> actual_documents;
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            actual_documents = std::move(documents);
            quit_closure.Run();
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items",
      R"(
        {}
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(actual_documents.empty());
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("drive")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Drive.ItemSuggestRequestResult",
                   ItemSuggestRequestResult::kContentError));
}

TEST_F(DriveServiceTest, DismissModule) {
  service_->DismissModule();
  EXPECT_EQ(base::Time::Now(),
            prefs_.GetTime(DriveService::kLastDismissedTimePrefName));
}

TEST_F(DriveServiceTest, RestoreModule) {
  service_->RestoreModule();
  EXPECT_EQ(base::Time(),
            prefs_.GetTime(DriveService::kLastDismissedTimePrefName));
}

class DriveServiceFakeDataTest : public DriveServiceTest {
 public:
  DriveServiceFakeDataTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpDriveModule, {{"NtpDriveModuleDataParam", "fake"}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(DriveServiceFakeDataTest, ReturnsFakeData) {
  std::vector<file_suggestion::mojom::FilePtr> fake_documents;
  base::MockCallback<DriveService::GetFilesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<file_suggestion::mojom::FilePtr> documents) {
            fake_documents = std::move(documents);
          }));

  prefs_.SetTime(DriveService::kLastDismissedTimePrefName, base::Time::Now());
  task_environment_.AdvanceClock(DriveService::kDismissDuration);
  service_->GetDriveFiles(callback.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(fake_documents.empty());
}

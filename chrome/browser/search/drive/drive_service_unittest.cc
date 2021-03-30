// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
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
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();
    service_ = std::make_unique<DriveService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env.identity_manager());
    identity_test_env.MakePrimaryAccountAvailable("example@google.com");
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
};

TEST_F(DriveServiceTest, PassesDataOnSuccess) {
  std::vector<drive::mojom::FilePtr> actual_documents;
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<drive::mojom::FilePtr> documents) {
            actual_documents = std::move(documents);
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

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
                "displayText": {
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
                "displayText": {
                  "textSegment": [
                    {
                      "text": "Foo "
                    },
                    {
                      "text": "bar foo bar"
                    }
                  ]
                },
                "primaryPerson": {
                  "photoUrl": "https://google.com/userphoto"
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
  EXPECT_EQ("https://google.com/userphoto",
            actual_documents.at(1)->untrusted_photo_url.value());
}

TEST_F(DriveServiceTest, PassesDataToMultipleRequestsToDriveService) {
  std::vector<drive::mojom::FilePtr> response1;
  std::vector<drive::mojom::FilePtr> response2;
  std::vector<drive::mojom::FilePtr> response3;
  std::vector<drive::mojom::FilePtr> response4;

  base::MockCallback<DriveService::GetFilesCallback> callback1;
  base::MockCallback<DriveService::GetFilesCallback> callback2;
  base::MockCallback<DriveService::GetFilesCallback> callback3;
  base::MockCallback<DriveService::GetFilesCallback> callback4;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<drive::mojom::FilePtr> documents) {
            response1 = std::move(documents);
          }));
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<drive::mojom::FilePtr> documents) {
            response2 = std::move(documents);
          }));
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<drive::mojom::FilePtr> documents) {
            response3 = std::move(documents);
          }));
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<drive::mojom::FilePtr> documents) {
            response4 = std::move(documents);
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
                "displayText": {
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
}

TEST_F(DriveServiceTest, PassesNoDataOnAuthError) {
  bool token_is_valid = true;

  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&token_is_valid](std::vector<drive::mojom::FilePtr> suggestions) {
            token_is_valid = !suggestions.empty();
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  EXPECT_FALSE(token_is_valid);
}

TEST_F(DriveServiceTest, PassesNoDataOnNetError) {
  bool empty_response = false;
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&empty_response](std::vector<drive::mojom::FilePtr> suggestions) {
            empty_response = suggestions.empty();
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

  EXPECT_TRUE(empty_response);
}

TEST_F(DriveServiceTest, PassesNoDataOnEmptyResponse) {
  bool empty_response = false;

  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&empty_response](std::vector<drive::mojom::FilePtr> suggestions) {
            empty_response = suggestions.empty();
          }));

  service_->GetDriveFiles(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", "", net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);

  EXPECT_TRUE(empty_response);
}

TEST_F(DriveServiceTest, PassesNoDataOnMissingItemKey) {
  std::vector<drive::mojom::FilePtr> actual_documents;
  base::MockCallback<DriveService::GetFilesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<drive::mojom::FilePtr> documents) {
            actual_documents = std::move(documents);
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

  EXPECT_TRUE(actual_documents.empty());
}

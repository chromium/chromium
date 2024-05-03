// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_image_service/android/image_service_bridge.h"

#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockImageService : public page_image_service::ImageService {
 public:
  MOCK_METHOD(void,
              FetchImageFor,
              (page_image_service::mojom::ClientId,
               const GURL&,
               const page_image_service::mojom::Options&,
               ResultCallback),
              (override));

  MOCK_METHOD(base::WeakPtr<page_image_service::ImageService>,
              GetWeakPtr,
              (),
              (override));
};

// Unit tests for `ImageServiceBridge`.
class ImageServiceBridgeTest : public testing::Test {
 public:
  ImageServiceBridgeTest() = default;
  ~ImageServiceBridgeTest() override = default;

  // testing::Test
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("ImageServiceBridgeTest",
                                                      /*testing_factories=*/{});
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();

    image_service_bridge_ = std::make_unique<ImageServiceBridge>(
        &mock_image_service_, identity_test_environment_->identity_manager());
  }

  void TearDown() override { image_service_bridge_.reset(); }

  ImageServiceBridge* image_service_bridge() {
    return image_service_bridge_.get();
  }

  signin::IdentityTestEnvironment* identity_test_environment() {
    return identity_test_environment_.get();
  }

  MockImageService& mock_image_service() { return mock_image_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ImageServiceBridge> image_service_bridge_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;

  raw_ptr<Profile> profile_;

  MockImageService mock_image_service_;
};

TEST_F(ImageServiceBridgeTest, TestGetImageUrl) {
  identity_test_environment()->SetPrimaryAccount("test@gmail.com",
                                                 signin::ConsentLevel::kSync);

  GURL url = GURL("http://foo.com");
  // This callback will only be invoked for edge cases. Nothing will happen
  // when the mock_image_service() is called.
  base::MockOnceCallback<void(const GURL&)> mock_callback;

  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  EXPECT_CALL(mock_image_service(), FetchImageFor(testing::_, testing::Eq(url),
                                                  testing::_, testing::_));
  image_service_bridge()->FetchImageUrlForImpl(
      /*is_account_data=*/false, page_image_service::mojom::ClientId::Bookmarks,
      url, mock_callback.Get());

  // Without sync consent, no call will be made for locally-tied data
  // (is_account_data is false).
  identity_test_environment()->ClearPrimaryAccount();
  identity_test_environment()->SetPrimaryAccount("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  EXPECT_CALL(mock_callback, Run(testing::Eq(GURL())));
  EXPECT_CALL(mock_image_service(),
              FetchImageFor(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  image_service_bridge()->FetchImageUrlForImpl(
      /*is_account_data=*/false, page_image_service::mojom::ClientId::Bookmarks,
      url, mock_callback.Get());

  // When the page being fetched is tied to account data, the sync consent
  // won't matter.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  EXPECT_CALL(mock_image_service(), FetchImageFor(testing::_, testing::Eq(url),
                                                  testing::_, testing::_));
  image_service_bridge()->FetchImageUrlForImpl(
      /*is_account_data=*/true, page_image_service::mojom::ClientId::Bookmarks,
      url, mock_callback.Get());
}

TEST_F(ImageServiceBridgeTest, TestGetImageUrlWithInvalidURL) {
  base::MockOnceCallback<void(const GURL&)> mock_callback;
  EXPECT_CALL(mock_callback, Run(GURL())).Times(1);
  // When called with an invalid GURL, an empty GURL will be returned.
  image_service_bridge()->FetchImageUrlForImpl(
      /*is_account_data=*/false, page_image_service::mojom::ClientId::Bookmarks,
      GURL(""), mock_callback.Get());
}

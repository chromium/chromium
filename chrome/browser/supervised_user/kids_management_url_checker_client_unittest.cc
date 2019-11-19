// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_management_url_checker_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace {

using kids_chrome_management::ClassifyUrlResponse;

ClassifyUrlResponse::DisplayClassification ConvertClassification(
    safe_search_api::ClientClassification classification) {
  switch (classification) {
    case safe_search_api::ClientClassification::kAllowed:
      return ClassifyUrlResponse::ALLOWED;
    case safe_search_api::ClientClassification::kRestricted:
      return ClassifyUrlResponse::RESTRICTED;
    case safe_search_api::ClientClassification::kUnknown:
      return ClassifyUrlResponse::UNKNOWN_DISPLAY_CLASSIFICATION;
  }
}

// Build fake response proto with a response according to |classification|.
std::unique_ptr<ClassifyUrlResponse> BuildResponseProto(
    safe_search_api::ClientClassification classification) {
  auto response_proto = std::make_unique<ClassifyUrlResponse>();

  response_proto->set_display_classification(
      ConvertClassification(classification));
  return response_proto;
}

class KidsChromeManagementClientForTesting : public KidsChromeManagementClient {
 public:
  explicit KidsChromeManagementClientForTesting(
      content::BrowserContext* context)
      : KidsChromeManagementClient(static_cast<Profile*>(context)) {}

  ~KidsChromeManagementClientForTesting() override = default;

  void ClassifyURL(
      std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
      KidsChromeManagementClient::KidsChromeManagementCallback callback)
      override {
    std::move(callback).Run(std::move(response_proto_), error_code_);
  }

  void SetupResponse(std::unique_ptr<ClassifyUrlResponse> response_proto,
                     KidsChromeManagementClient::ErrorCode error_code) {
    response_proto_ = std::move(response_proto);
    error_code_ = error_code;
  }

 private:
  std::unique_ptr<ClassifyUrlResponse> response_proto_;
  KidsChromeManagementClient::ErrorCode error_code_;

  DISALLOW_COPY_AND_ASSIGN(KidsChromeManagementClientForTesting);
};

std::unique_ptr<KeyedService> CreateKidsChromeManagementClient(
    content::BrowserContext* context) {
  return std::make_unique<KidsChromeManagementClientForTesting>(context);
}

}  // namespace

class KidsManagementURLCheckerClientTest : public testing::Test {
 public:
  KidsManagementURLCheckerClientTest() = default;
  void SetUp() override {
    test_profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(test_profile_manager_->SetUp());

// ChromeOS requires a chromeos::FakeChromeUserManager for the tests to work.
#if defined(OS_CHROMEOS)
    const char kEmail[] = "account@gmail.com";
    const AccountId test_account_id(AccountId::FromUserEmail(kEmail));
    user_manager_ = new chromeos::FakeChromeUserManager;
    user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);
    user_manager_->SwitchActiveUser(test_account_id);
    test_profile_ = test_profile_manager_->CreateTestingProfile(
        test_account_id.GetUserEmail());

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_));
#else
    test_profile_ =
        test_profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
#endif

    DCHECK(test_profile_);

    KidsChromeManagementClientFactory::GetInstance()->SetTestingFactory(
        test_profile_, base::BindRepeating(&CreateKidsChromeManagementClient));

    url_classifier_ = std::make_unique<KidsManagementURLCheckerClient>("us");
  }

 protected:
  void SetupClientResponse(std::unique_ptr<ClassifyUrlResponse> response_proto,
                           KidsChromeManagementClient::ErrorCode error_code) {
    static_cast<KidsChromeManagementClientForTesting*>(
        KidsChromeManagementClientFactory::GetInstance()->GetForBrowserContext(
            test_profile_))
        ->SetupResponse(std::move(response_proto), error_code);
  }

  void CheckURL(const GURL& url) {
    url_classifier_->CheckURL(
        url, base::BindOnce(&KidsManagementURLCheckerClientTest::OnCheckDone,
                            base::Unretained(this)));
  }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url,
                    safe_search_api::ClientClassification classification));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* test_profile_;
  std::unique_ptr<TestingProfileManager> test_profile_manager_;
  std::unique_ptr<KidsManagementURLCheckerClient> url_classifier_;
#if defined(OS_CHROMEOS)
  chromeos::FakeChromeUserManager* user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(KidsManagementURLCheckerClientTest);
};

TEST_F(KidsManagementURLCheckerClientTest, Simple) {
  {
    GURL url("http://randomurl1.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kAllowed;

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kSuccess);

    CheckURL(url);
  }
  {
    GURL url("http://randomurl2.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kRestricted;

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kSuccess);
    CheckURL(url);
  }
}

TEST_F(KidsManagementURLCheckerClientTest, AccessTokenError) {
  GURL url("http://randomurl3.com");

  safe_search_api::ClientClassification classification =
      safe_search_api::ClientClassification::kUnknown;

  SetupClientResponse(BuildResponseProto(classification),
                      KidsChromeManagementClient::ErrorCode::kTokenError);

  EXPECT_CALL(*this, OnCheckDone(url, classification));
  CheckURL(url);
}

TEST_F(KidsManagementURLCheckerClientTest, NetworkErrors) {
  {
    GURL url("http://randomurl4.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kUnknown;

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kNetworkError);

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    CheckURL(url);
  }

  {
    GURL url("http://randomurl5.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kUnknown;

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kHttpError);

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    CheckURL(url);
  }
}

TEST_F(KidsManagementURLCheckerClientTest, ServiceError) {
  GURL url("http://randomurl6.com");

  safe_search_api::ClientClassification classification =
      safe_search_api::ClientClassification::kUnknown;

  SetupClientResponse(BuildResponseProto(classification),
                      KidsChromeManagementClient::ErrorCode::kServiceError);

  EXPECT_CALL(*this, OnCheckDone(url, classification));
  CheckURL(url);
}

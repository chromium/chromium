// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/socs_cookie_fetcher.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::test {
namespace {
constexpr char kEmail[] = "test-user@example.com";
constexpr char16_t kEmail16[] = u"test-user@example.com";
const char kEssentialSearchURL[] =
    "https://chromeoscompliance-pa.googleapis.com/v1/essentialsearch/"
    "socscookieheader?key=%s";
}  // namespace

class SocsCookieFetcherConsumerTest
    : public app_list::SocsCookieFetcher::Consumer {
 public:
  SocsCookieFetcherConsumerTest();
  ~SocsCookieFetcherConsumerTest() override;

  // SocsCookieFetcher::Consumer
  void OnCookieFetched(const std::string& cookie_header) override;
  void OnApiCallFailed(app_list::SocsCookieFetcher::Status status) override;

  bool CookieFetched();
  app_list::SocsCookieFetcher::Status GetStatus();

 private:
  bool cookie_fetched_;
  app_list::SocsCookieFetcher::Status status_;
};

SocsCookieFetcherConsumerTest::SocsCookieFetcherConsumerTest()
    : cookie_fetched_(false) {}

SocsCookieFetcherConsumerTest::~SocsCookieFetcherConsumerTest() = default;

void SocsCookieFetcherConsumerTest::OnCookieFetched(
    const std::string& cookie_header) {
  cookie_fetched_ = true;
}

void SocsCookieFetcherConsumerTest::OnApiCallFailed(
    app_list::SocsCookieFetcher::Status status) {
  status_ = status;
}

bool SocsCookieFetcherConsumerTest::CookieFetched() {
  return cookie_fetched_;
}

app_list::SocsCookieFetcher::Status SocsCookieFetcherConsumerTest::GetStatus() {
  return status_;
}

class SocsCookieFetcherTest : public testing::Test {
 protected:
  SocsCookieFetcherTest();
  ~SocsCookieFetcherTest() override;

  // testing::Test:
  void SetUp() override;

  void CreateSocsCookieFetcher();
  void DestroySocsCookieFetcher();

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<SocsCookieFetcherConsumerTest> consumer_;
  std::unique_ptr<app_list::SocsCookieFetcher> socs_cookie_fetcher_;

  network::TestURLLoaderFactory url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

SocsCookieFetcherTest::SocsCookieFetcherTest()
    : consumer_(nullptr), socs_cookie_fetcher_(nullptr) {}

SocsCookieFetcherTest::~SocsCookieFetcherTest() {
  DestroySocsCookieFetcher();
}

void SocsCookieFetcherTest::SetUp() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());
  profile_ = profile_manager_->CreateTestingProfile(
      kEmail, /*prefs=*/{}, kEmail16,
      /*avatar_id=*/0,
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory_)}}));

  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  identity_test_env_ = identity_test_env_adaptor_->identity_test_env();
  identity_test_env_->SetTestURLLoaderFactory(&url_loader_factory_);
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_loader_factory_);
}

void SocsCookieFetcherTest::CreateSocsCookieFetcher() {
  DestroySocsCookieFetcher();
  consumer_ = std::make_unique<SocsCookieFetcherConsumerTest>();
  socs_cookie_fetcher_ = std::make_unique<app_list::SocsCookieFetcher>(
      shared_url_loader_factory_, consumer_.get());
}

void SocsCookieFetcherTest::DestroySocsCookieFetcher() {
  if (socs_cookie_fetcher_) {
    socs_cookie_fetcher_ = nullptr;
    consumer_ = nullptr;
  }
}

TEST_F(SocsCookieFetcherTest, FetchSocsCookieSucceed) {
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  CreateSocsCookieFetcher();

  constexpr char kValidJsonResponse[] = R"(
    {
        "cookieHeader": "SOCS=socs_cookie;"
    })";

  std::string essential_search_url =
      base::StringPrintf(kEssentialSearchURL, google_apis::GetAPIKey().c_str());
  url_loader_factory_.AddResponse(essential_search_url, kValidJsonResponse,
                                  net::HTTP_OK);

  socs_cookie_fetcher_->StartFetching();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(consumer_->CookieFetched());
}

}  // namespace ash::test

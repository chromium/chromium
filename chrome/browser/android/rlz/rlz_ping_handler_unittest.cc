// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/rlz/rlz_ping_handler.h"

#include <string>

#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "rlz/lib/lib_values.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome::android {

namespace {

constexpr char kTestBrand[] = "BRND";
constexpr char kTestLanguage[] = "en";
constexpr char kTestEvents[] = "I7S,Y0S";

std::string MakeTestId() {
  return std::string(50, 'A');
}

}  // namespace

class RlzPingHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(RlzPingHandlerTest, PingUsesSharedURLLoaderFactoryAndValidatesResponse) {
  base::test::TestFuture<bool> result;
  auto* handler = new RlzPingHandler();
  handler->Ping(kTestBrand, kTestLanguage, kTestEvents, MakeTestId(),
                result.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(request.url.GetHost(), rlz_lib::kFinancialServer);
  EXPECT_EQ(request.url.GetPath(), rlz_lib::kFinancialPingPath);
  EXPECT_EQ(request.load_flags, net::LOAD_DISABLE_CACHE);
  EXPECT_EQ(request.credentials_mode, network::mojom::CredentialsMode::kOmit);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      request.url, rlz_lib::kProductSignatureCgiVariable, &value));
  EXPECT_EQ(value, "chrome");
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      request.url, rlz_lib::kProductBrandCgiVariable, &value));
  EXPECT_EQ(value, kTestBrand);
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      request.url, rlz_lib::kProductLanguageCgiVariable, &value));
  EXPECT_EQ(value, kTestLanguage);
  EXPECT_TRUE(net::GetValueForKeyInQuery(request.url,
                                         rlz_lib::kEventsCgiVariable, &value));
  EXPECT_EQ(value, kTestEvents);
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      request.url, rlz_lib::kMachineIdCgiVariable, &value));
  EXPECT_EQ(value, MakeTestId());

  // CRC-32 of an empty response body is 0.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      request.url.spec(), "crc32: 0\n"));
  EXPECT_TRUE(result.Get());
}

TEST_F(RlzPingHandlerTest, PingFailsWithInvalidChecksum) {
  base::test::TestFuture<bool> result;
  auto* handler = new RlzPingHandler();
  handler->Ping(kTestBrand, kTestLanguage, kTestEvents, MakeTestId(),
                result.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const GURL url = test_url_loader_factory_.GetPendingRequest(0)->request.url;
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), "invalid response"));
  EXPECT_FALSE(result.Get());
}

TEST_F(RlzPingHandlerTest, PingFailsWithHttpError) {
  base::test::TestFuture<bool> result;
  auto* handler = new RlzPingHandler();
  handler->Ping(kTestBrand, kTestLanguage, kTestEvents, MakeTestId(),
                result.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const GURL url = test_url_loader_factory_.GetPendingRequest(0)->request.url;
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), "", net::HTTP_NOT_FOUND));
  EXPECT_FALSE(result.Get());
}

TEST_F(RlzPingHandlerTest, PingShortCircuitsOnEmptyParam) {
  base::test::TestFuture<bool> result;
  auto* handler = new RlzPingHandler();
  handler->Ping("", kTestLanguage, kTestEvents, MakeTestId(),
                result.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(result.Get());
}

}  // namespace chrome::android

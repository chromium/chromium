// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/cloud_ap_provider_win.h"

#include <proofofpossessioncookieinfo.h>

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;

namespace enterprise_auth {

namespace {

// Helper function for constructing ProofOfPossessionCookieInfo objects.
ProofOfPossessionCookieInfo GetCookieInfo(const wchar_t* name,
                                          const wchar_t* data) {
  ProofOfPossessionCookieInfo cookie_info;
  cookie_info.name = const_cast<LPWSTR>(name);
  cookie_info.data = const_cast<LPWSTR>(data);
  return cookie_info;
}

}  // namespace

class CloudApProviderWinTest : public ::testing::Test {
 protected:
  ~CloudApProviderWinTest() override {
    // Clear an override of the join type made by any test.
    CloudApProviderWin::SetSupportLevelForTesting(std::nullopt);
  }

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));

    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kIdentityStorePath,
                         KEY_WOW64_64KEY | KEY_SET_VALUE),
              ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(kLoginUriName, L"https://host1"), ERROR_SUCCESS);

    ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kPackagePath,
                         KEY_WOW64_64KEY | KEY_SET_VALUE),
              ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(kLoginUriName, L"https://host2"), ERROR_SUCCESS);
  }

  static const wchar_t kIdentityStorePath[];
  static const wchar_t kPackagePath[];
  static const wchar_t kLoginUriName[];

 private:
  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_;
};

// static
constexpr wchar_t CloudApProviderWinTest::kIdentityStorePath[] =
    L"SOFTWARE\\Microsoft\\IdentityStore\\LoadParameters\\"
    L"{B16898C6-A148-4967-9171-64D755DA8520}";

// static
constexpr wchar_t CloudApProviderWinTest::kPackagePath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\AAD\\Package";

// static
constexpr wchar_t CloudApProviderWinTest::kLoginUriName[] = L"LoginUri";

// Tests that the provider returns null when AAD SSO is not supported.
TEST_F(CloudApProviderWinTest, Unsupported) {
  CloudApProviderWin::SetSupportLevelForTesting(
      CloudApProviderWin::SupportLevel::kUnsupported);

  CloudApProviderWin provider;

  base::RunLoop run_loop;
  base::MockCallback<CloudApProviderWin::FetchOriginsCallback> mock;
  EXPECT_CALL(mock, Run(_))
      .WillOnce([&run_loop](std::unique_ptr<std::vector<url::Origin>> origins) {
        run_loop.Quit();
        EXPECT_EQ(origins.get(), nullptr);
      });

  provider.FetchOrigins(mock.Get());
  run_loop.Run();
}

// Tests that the provider returns an empty set of origins when the machine
// isn't joined to an AAD domain.
TEST_F(CloudApProviderWinTest, NotJoined) {
  CloudApProviderWin::SetSupportLevelForTesting(
      CloudApProviderWin::SupportLevel::kDisabled);

  CloudApProviderWin provider;

  base::RunLoop run_loop;
  base::MockCallback<CloudApProviderWin::FetchOriginsCallback> mock;
  EXPECT_CALL(mock, Run(_))
      .WillOnce([&run_loop](std::unique_ptr<std::vector<url::Origin>> origins) {
        run_loop.Quit();
        ASSERT_NE(origins.get(), nullptr);
        EXPECT_TRUE(origins->empty());
      });

  provider.FetchOrigins(mock.Get());
  run_loop.Run();
}

// Tests that the provider returns the two origins in the registry when the
// machine is joined to an AAD domain.
TEST_F(CloudApProviderWinTest, Joined) {
  CloudApProviderWin::SetSupportLevelForTesting(
      CloudApProviderWin::SupportLevel::kEnabled);

  CloudApProviderWin provider;

  base::RunLoop run_loop;
  base::MockCallback<CloudApProviderWin::FetchOriginsCallback> mock;
  EXPECT_CALL(mock, Run(_))
      .WillOnce([&run_loop](std::unique_ptr<std::vector<url::Origin>> origins) {
        run_loop.Quit();
        ASSERT_NE(origins.get(), nullptr);
        EXPECT_EQ(*origins, std::vector<url::Origin>(
                                {url::Origin::Create(GURL("https://host1")),
                                 url::Origin::Create(GURL("https://host2"))}));
      });

  provider.FetchOrigins(mock.Get());
  run_loop.Run();
}

// Tests that the provider doesn't crash when the actual provider detection is
// run.
TEST_F(CloudApProviderWinTest, Platform) {
  CloudApProviderWin provider;

  base::RunLoop run_loop;
  base::MockCallback<CloudApProviderWin::FetchOriginsCallback> mock;
  EXPECT_CALL(mock, Run(_))
      .WillOnce([&run_loop](std::unique_ptr<std::vector<url::Origin>> origins) {
        run_loop.Quit();
      });

  provider.FetchOrigins(mock.Get());
  run_loop.Run();
}

// Tests that cookie info is correctly parsed into the corresponding headers.
TEST_F(CloudApProviderWinTest, ParseCookieInfo) {
  CloudApProviderWin provider;
  net::HttpRequestHeaders auth_headers;
  DWORD cookie_info_count = 2;

  const wchar_t* cookie_name_1 = L"name";
  const wchar_t* cookie_name_2 = L"x-ms-name";
  const wchar_t* cookie_data = L"data; cookie_attributes; a; b";

  ProofOfPossessionCookieInfo cookie_info_1 =
      GetCookieInfo(cookie_name_1, cookie_data);
  ProofOfPossessionCookieInfo cookie_info_2 =
      GetCookieInfo(cookie_name_2, cookie_data);
  ProofOfPossessionCookieInfo cookie_info[2] = {cookie_info_1, cookie_info_2};
  provider.ParseCookieInfoForTesting(cookie_info, cookie_info_count,
                                     auth_headers);

  // The names and data of all cookies whose names don't begin with 'x-ms'
  // should appear in the cookie header.
  EXPECT_EQ(auth_headers.GetHeader(net::HttpRequestHeaders::kCookie),
            base::JoinString({base::WideToASCII(cookie_name_1),
                              base::WideToASCII(cookie_data)},
                             "="));

  // Only cookies whose name begins with 'x-ms' should be set as individual
  // headers.
  EXPECT_FALSE(auth_headers.GetHeader(base::WideToASCII(cookie_name_1)));
  // Cookie attributes are removed from the header value.
  EXPECT_EQ(auth_headers.GetHeader(base::WideToASCII(cookie_name_2)),
            base::WideToASCII(L"data"));
}

}  // namespace enterprise_auth

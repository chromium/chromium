// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/cloud_ap_utils_win.h"

#include <windows.h>

#include <vector>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_auth {

class AppendRegistryOriginsTest : public ::testing::Test {
 protected:
  AppendRegistryOriginsTest() = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

// Test that an origin is extracted from an SZ value.
TEST_F(AppendRegistryOriginsTest, SingleSz) {
  static constexpr wchar_t kRawValue[] = L"https://one";

  ASSERT_EQ(
      base::win::RegKey(HKEY_CURRENT_USER, L"TestKey", KEY_SET_VALUE)
          .WriteValue(L"TestValue", &kRawValue[0], sizeof(kRawValue), REG_SZ),
      ERROR_SUCCESS);

  std::vector<url::Origin> result;
  AppendRegistryOrigins(HKEY_CURRENT_USER, L"TestKey", L"TestValue", result);
  EXPECT_EQ(result, std::vector<url::Origin>(
                        {url::Origin::Create(GURL("https://one"))}));
}

// Tests that existing values are preserved when extracting a single value.
TEST_F(AppendRegistryOriginsTest, SingleSzAppend) {
  static constexpr wchar_t kRawValue[] = L"https://two";

  ASSERT_EQ(
      base::win::RegKey(HKEY_CURRENT_USER, L"TestKey", KEY_SET_VALUE)
          .WriteValue(L"TestValue", &kRawValue[0], sizeof(kRawValue), REG_SZ),
      ERROR_SUCCESS);

  std::vector<url::Origin> result{url::Origin::Create(GURL("https://one"))};
  AppendRegistryOrigins(HKEY_CURRENT_USER, L"TestKey", L"TestValue", result);
  EXPECT_EQ(result, std::vector<url::Origin>(
                        {url::Origin::Create(GURL("https://one")),
                         url::Origin::Create(GURL("https://two"))}));
}

// Test that several values are extracted from a MUTLI_SZ value.
TEST_F(AppendRegistryOriginsTest, MultiSz) {
  static constexpr wchar_t kRawValue[] =
      L"https://one\0https://two\0https://three\0";

  ASSERT_EQ(base::win::RegKey(HKEY_CURRENT_USER, L"TestKey", KEY_SET_VALUE)
                .WriteValue(L"TestValue", &kRawValue[0], sizeof(kRawValue),
                            REG_MULTI_SZ),
            ERROR_SUCCESS);

  std::vector<url::Origin> result;
  AppendRegistryOrigins(HKEY_CURRENT_USER, L"TestKey", L"TestValue", result);
  EXPECT_EQ(result, std::vector<url::Origin>(
                        {url::Origin::Create(GURL("https://one")),
                         url::Origin::Create(GURL("https://two")),
                         url::Origin::Create(GURL("https://three"))}));
}

// Test that existing values are preserved when extracting multiple values.
TEST_F(AppendRegistryOriginsTest, MultiSzAppend) {
  static constexpr wchar_t kRawValue[] =
      L"https://one\0https://two\0https://three\0";

  ASSERT_EQ(base::win::RegKey(HKEY_CURRENT_USER, L"TestKey", KEY_SET_VALUE)
                .WriteValue(L"TestValue", &kRawValue[0], sizeof(kRawValue),
                            REG_MULTI_SZ),
            ERROR_SUCCESS);

  std::vector<url::Origin> result{url::Origin::Create(GURL("https://zero"))};
  AppendRegistryOrigins(HKEY_CURRENT_USER, L"TestKey", L"TestValue", result);
  EXPECT_EQ(result, std::vector<url::Origin>(
                        {url::Origin::Create(GURL("https://zero")),
                         url::Origin::Create(GURL("https://one")),
                         url::Origin::Create(GURL("https://two")),
                         url::Origin::Create(GURL("https://three"))}));
}

// Test that a bogus value is ignored in an SZ value.
TEST_F(AppendRegistryOriginsTest, SingleBogus) {
  static constexpr wchar_t kRawValue[] = L"musiciscool";

  ASSERT_EQ(base::win::RegKey(HKEY_CURRENT_USER, L"TestKey", KEY_SET_VALUE)
                .WriteValue(L"TestValue", &kRawValue[0], sizeof(kRawValue),
                            REG_MULTI_SZ),
            ERROR_SUCCESS);

  std::vector<url::Origin> result;
  AppendRegistryOrigins(HKEY_CURRENT_USER, L"TestKey", L"TestValue", result);
  EXPECT_EQ(result, std::vector<url::Origin>());
}

// Test that bogus values are ignored in a MULTI_SZ value.
TEST_F(AppendRegistryOriginsTest, MultiSzWithBogus) {
  static constexpr wchar_t kRawValue[] =
      L"https://one\0musiciscool\0https://two\0";

  ASSERT_EQ(base::win::RegKey(HKEY_CURRENT_USER, L"TestKey", KEY_SET_VALUE)
                .WriteValue(L"TestValue", &kRawValue[0], sizeof(kRawValue),
                            REG_MULTI_SZ),
            ERROR_SUCCESS);

  std::vector<url::Origin> result;
  AppendRegistryOrigins(HKEY_CURRENT_USER, L"TestKey", L"TestValue", result);
  EXPECT_EQ(result, std::vector<url::Origin>(
                        {url::Origin::Create(GURL("https://one")),
                         url::Origin::Create(GURL("https://two"))}));
}

}  // namespace enterprise_auth

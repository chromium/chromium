// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/auto_login_parser.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AutoLoginParserTest : public testing::Test {
 protected:
  static bool IsHeaderDataEmpty(const HeaderData& header) {
    return header.realm.empty() && header.account.empty() &&
        header.args.empty();
  }
};

TEST_F(AutoLoginParserTest, ParseHeader) {
  std::string header =
      "realm=com.google&"
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  HeaderData header_data;
  EXPECT_TRUE(ParseHeader(header, ONLY_GOOGLE_COM, &header_data));

  ASSERT_EQ("com.google", header_data.realm);
  ASSERT_EQ("fred.example@gmail.com", header_data.account);
  ASSERT_EQ("kfdshfwoeriudslkfsdjfhdskjfhsdkr", header_data.args);
}

TEST_F(AutoLoginParserTest, ParseHeaderOnlySupportsComGoogle) {
  std::string header =
      "realm=com.microsoft&"
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  HeaderData header_data;
  EXPECT_FALSE(ParseHeader(header, ONLY_GOOGLE_COM, &header_data));
  // |header| should not be touched when parsing fails.
  EXPECT_TRUE(IsHeaderDataEmpty(header_data));
}

TEST_F(AutoLoginParserTest, ParseHeaderWithMissingRealm) {
  std::string header =
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  HeaderData header_data;
  EXPECT_FALSE(ParseHeader(header, ONLY_GOOGLE_COM, &header_data));
  EXPECT_TRUE(IsHeaderDataEmpty(header_data));
}

TEST_F(AutoLoginParserTest, ParseHeaderWithMissingArgs) {
  std::string header =
      "realm=com.google&"
      "account=fred.example%40gmail.com&";

  HeaderData header_data;
  EXPECT_FALSE(ParseHeader(header, ONLY_GOOGLE_COM, &header_data));
  EXPECT_TRUE(IsHeaderDataEmpty(header_data));
}

TEST_F(AutoLoginParserTest, ParseHeaderWithoutOptionalAccount) {
  std::string header =
      "realm=com.google&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  HeaderData header_data;
  EXPECT_TRUE(ParseHeader(header, ONLY_GOOGLE_COM, &header_data));
  ASSERT_EQ("com.google", header_data.realm);
  ASSERT_EQ("kfdshfwoeriudslkfsdjfhdskjfhsdkr", header_data.args);
}

TEST_F(AutoLoginParserTest, ParseHeaderAllowsAnyRealmWithOption) {
  std::string header =
      "realm=com.microsoft&"
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  HeaderData header_data;
  EXPECT_TRUE(ParseHeader(header, ALLOW_ANY_REALM, &header_data));

  ASSERT_EQ("com.microsoft", header_data.realm);
  ASSERT_EQ("fred.example@gmail.com", header_data.account);
  ASSERT_EQ("kfdshfwoeriudslkfsdjfhdskjfhsdkr", header_data.args);
}

}  // namespace android_webview

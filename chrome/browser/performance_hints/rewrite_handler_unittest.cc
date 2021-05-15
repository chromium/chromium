// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/rewrite_handler.h"

#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace performance_hints {

TEST(RewriteHandlerTest, ExtraQueryParams) {
  RewriteHandler handler =
      RewriteHandler::FromConfigString("www.google.com/url?url");

  GURL url(
      "https://www.google.com/url?not=used&url=https://theactualurl.com/"
      "testpath?testquerytoo=true&unusedparamfromouterurl");
  absl::optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath?testquerytoo=true",
            result.value().spec());
}

TEST(RewriteHandlerTest, EscapedCharacters) {
  RewriteHandler handler =
      RewriteHandler::FromConfigString("www.google.com/url?url");

  GURL url(
      "https://www.google.com/url?url=https://theactualurl.com/"
      "testpath?first=param%26second=param&unusedparamfromouterurl");
  absl::optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath?first=param&second=param",
            result.value().spec());
}

TEST(RewriteHandlerTest, NoMatchingParam) {
  RewriteHandler handler =
      RewriteHandler::FromConfigString("www.google.com/url?url");

  GURL url(
      "https://www.google.com/url?notactuallyurl=https://theactualurl.com");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(RewriteHandlerTest, InvalidUrl) {
  RewriteHandler handler =
      RewriteHandler::FromConfigString("www.google.com/url?url");

  GURL url("invalid");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(RewriteHandlerTest, EmptyConfig) {
  RewriteHandler handler = RewriteHandler::FromConfigString("");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(RewriteHandlerTest, NoQueryParam) {
  RewriteHandler handler =
      RewriteHandler::FromConfigString("www.google.com/url");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(RewriteHandlerTest, NoHostPath) {
  RewriteHandler handler = RewriteHandler::FromConfigString("?url");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(RewriteHandlerTest, HostOnly) {
  RewriteHandler handler =
      RewriteHandler::FromConfigString("www.google.com/?url");

  GURL url("https://www.google.com?url=https://theactualurl.com/testpath");
  absl::optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath", result.value().spec());
}

TEST(RewriteHandlerTest, MultipleMatchers) {
  RewriteHandler handler = RewriteHandler::FromConfigString(
      "www.google.com/url?url,www.googleadservices.com/pagead/aclk?adurl");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  absl::optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath", result.value().spec());

  url = GURL(
      "https://www.googleadservices.com/pagead/aclk?adurl=https://"
      "theactualurl.com/testpath");
  result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath", result.value().spec());
}

}  // namespace performance_hints

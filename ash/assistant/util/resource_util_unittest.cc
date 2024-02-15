// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/resource_util.h"

#include <map>
#include <string>
#include <string_view>

#include "ash/assistant/util/test_support/macros.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Helpers ---------------------------------------------------------------------

std::string GetParamValue(const GURL& url, const std::string& param_key) {
  if (!url.has_query())
    return std::string();

  const std::string_view query_piece = url.query_piece();
  url::Component query(0, query_piece.length()), key, value;
  while (url::ExtractQueryKeyValue(query_piece, &query, &key, &value)) {
    if (query_piece.substr(key.begin, key.len) == param_key) {
      return std::string(query_piece.substr(value.begin, value.len));
    }
  }

  return std::string();
}

}  // namespace

// Tests -----------------------------------------------------------------------

using ResourceUtilTest = testing::Test;

TEST_F(ResourceUtilTest, AppendOrReplaceColorParam) {
  GURL icon_link("googleassistant://resource?type=icon");

  // Test append.
  icon_link = AppendOrReplaceColorParam(icon_link, SK_ColorBLACK);
  EXPECT_EQ(GetParamValue(icon_link, "color"), "ff000000");

  // Test replace.
  icon_link = AppendOrReplaceColorParam(icon_link, SK_ColorWHITE);
  EXPECT_EQ(GetParamValue(icon_link, "color"), "ffffffff");
}

TEST_F(ResourceUtilTest, CreateIconResourceLink) {
  // Test w/ color.
  EXPECT_EQ(CreateIconResourceLink(IconName::kAssistant, SK_ColorBLACK),
            GURL("googleassistant://"
                 "resource?type=icon&name=assistant&color=ff000000"));
  // Test w/o color.
  EXPECT_EQ(CreateIconResourceLink(IconName::kAssistant),
            GURL("googleassistant://resource?type=icon&name=assistant"));
}

TEST_F(ResourceUtilTest, CreateVectorIcon) {
  std::string url;
  gfx::ImageSkia actual, expected;

  // Test w/ name.
  url = "googleassistant://resource?type=icon&name=assistant";
  actual = CreateVectorIcon(GURL(url));
  expected =
      gfx::CreateVectorIcon(gfx::IconDescription(chromeos::kAssistantIcon));
  ASSERT_PIXELS_EQ(actual, expected);

  // Test w/ name and size.
  url = "googleassistant://resource?type=icon&name=assistant";
  actual = CreateVectorIcon(GURL(url), /*size=*/24);
  expected = gfx::CreateVectorIcon(
      gfx::IconDescription(chromeos::kAssistantIcon, /*size=*/24));
  ASSERT_PIXELS_EQ(actual, expected);

  // Test w/ name, size, and color.
  url = "googleassistant://resource?type=icon&name=assistant&color=ff000000";
  actual = CreateVectorIcon(GURL(url), /*size=*/24);
  expected = gfx::CreateVectorIcon(gfx::IconDescription(
      chromeos::kAssistantIcon, /*size=*/24, /*color=*/SK_ColorBLACK));
  ASSERT_PIXELS_EQ(actual, expected);

  // Test w/o icon resource link.
  actual = CreateVectorIcon(GURL("googleassistant://resource"));
  EXPECT_TRUE(actual.isNull());

  // Test w/o resource link.
  actual = CreateVectorIcon(GURL("https://g.co/"));
  EXPECT_TRUE(actual.isNull());

  // Test w/ empty.
  actual = CreateVectorIcon(GURL());
  EXPECT_TRUE(actual.isNull());
}

TEST_F(ResourceUtilTest, GetResourceLinkType) {
  const std::map<std::string, ResourceLinkType> test_cases = {
      // SUPPORTED:
      {"googleassistant://resource?type=icon", ResourceLinkType::kIcon},

      // UNSUPPORTED:
      {"googleassistant://resource", ResourceLinkType::kUnsupported},
      {"GOOGLEASSISTANT://RESOURCE?TYPE=ICON", ResourceLinkType::kUnsupported},
      {"https://g.co/", ResourceLinkType::kUnsupported},
      {"", ResourceLinkType::kUnsupported},
  };
  for (const auto& test_case : test_cases)
    EXPECT_EQ(GetResourceLinkType(GURL(test_case.first)), test_case.second);
}

TEST_F(ResourceUtilTest, IsResourceLinkType) {
  const std::map<std::string, ResourceLinkType> test_cases = {
      // SUPPORTED:
      {"googleassistant://resource?type=icon", ResourceLinkType::kIcon},

      // UNSUPPORTED:
      {"googleassistant://resource", ResourceLinkType::kUnsupported},
      {"GOOGLEASSISTANT://RESOURCE?TYPE=ICON", ResourceLinkType::kUnsupported},
      {"https://g.co/", ResourceLinkType::kUnsupported},
      {"", ResourceLinkType::kUnsupported},
  };
  for (const auto& test_case : test_cases)
    EXPECT_TRUE(IsResourceLinkType(GURL(test_case.first), test_case.second));
}

TEST_F(ResourceUtilTest, IsResourceLinkUrl) {
  const std::map<std::string, bool> test_cases = {
      // RESOURCE LINK:
      {"googleassistant://resource", true},
      {"googleassistant://resource?type=icon", true},

      // NOT RESOURCE LINK:
      {"GOOGLEASSISTANT://RESOURCE", false},
      {"https://g.co/", false},
      {"", false},
  };
  for (const auto& test_case : test_cases)
    EXPECT_EQ(IsResourceLinkUrl(GURL(test_case.first)), test_case.second);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash

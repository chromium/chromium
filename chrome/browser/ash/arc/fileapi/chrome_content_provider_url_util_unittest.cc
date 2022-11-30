// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/chrome_content_provider_url_util.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

TEST(ChromeContentProviderUrlUtilTest, EncodeAndDecode) {
  {
    const GURL original("externalfile://foo/bar/baz");
    GURL encoded = EncodeToChromeContentProviderUrl(original);
    EXPECT_TRUE(encoded.is_valid());
    GURL decoded = DecodeFromChromeContentProviderUrl(encoded);
    EXPECT_TRUE(decoded.is_valid());
    EXPECT_EQ(original, decoded);
  }
  {
    const GURL original(
        "externalfile://foo/!@#$%^&*()_+|~-=\\`[]{};':\"<>?,. /");
    GURL encoded = EncodeToChromeContentProviderUrl(original);
    EXPECT_TRUE(encoded.is_valid());
    GURL decoded = DecodeFromChromeContentProviderUrl(encoded);
    EXPECT_TRUE(decoded.is_valid());
    EXPECT_EQ(original, decoded);
  }
  {
    const std::u16string utf16_string = {
        0x307b,  // HIRAGANA_LETTER_HO
        0x3052,  // HIRAGANA_LETTER_GE
    };
    const GURL original("externalfile://foo/" +
                        base::UTF16ToUTF8(utf16_string));
    GURL encoded = EncodeToChromeContentProviderUrl(original);
    EXPECT_TRUE(encoded.is_valid());
    GURL decoded = DecodeFromChromeContentProviderUrl(encoded);
    EXPECT_TRUE(decoded.is_valid());
    EXPECT_EQ(original, decoded);
  }
}

}  // namespace arc

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/text_attachment.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TextAttachmentTextTitleTestData {
  TextAttachment::Type type;
  std::string text_body;
  std::string expected_text_title;
} kTextAttachmentTextTitleTestData[] = {
    {TextAttachment::Type::kText, "Short text", "Short text"},
    {TextAttachment::Type::kText, "Long text that should be truncated",
     "Long text that should be truncat…"},
    {TextAttachment::Type::kUrl,
     "https://www.google.com/maps/search/restaurants/@/"
     "data=!3m1!4b1?disco_ad=1234",
     "www.google.com"},
    {TextAttachment::Type::kUrl, "Invalid URL", "Invalid URL"},
    {TextAttachment::Type::kPhoneNumber, "1234", "xxxx"},
    {TextAttachment::Type::kPhoneNumber, "+1234", "+xxxx"},
    {TextAttachment::Type::kPhoneNumber, "123456", "12xxxx"},
    {TextAttachment::Type::kPhoneNumber, "12345678", "12xxxx78"},
    {TextAttachment::Type::kPhoneNumber, "+447123456789", "+44xxxxxx6789"},
    {TextAttachment::Type::kPhoneNumber,
     "+1255555555555555555555555555555555555555555555555556789",
     "+12xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6789"},
    {TextAttachment::Type::kPhoneNumber, "+16196784004", "+16xxxxx4004"},
    {TextAttachment::Type::kPhoneNumber, "+3841235782564", "+38xxxxxxx2564"},
    {TextAttachment::Type::kPhoneNumber, "+12345678901", "+12xxxxx8901"},
    {TextAttachment::Type::kPhoneNumber, "+1234567891", "+12xxxx7891"},
    {TextAttachment::Type::kPhoneNumber, "+123456789", "+12xxxx789"},
    {TextAttachment::Type::kPhoneNumber, "+12345678", "+12xxxx78"},
    {TextAttachment::Type::kPhoneNumber, "+1234567", "+12xxxx7"},
    {TextAttachment::Type::kPhoneNumber, "+123456", "+12xxxx"},
    {TextAttachment::Type::kPhoneNumber, "+12345", "+1xxxx"},
    {TextAttachment::Type::kPhoneNumber, "+1234", "+xxxx"},
    {TextAttachment::Type::kPhoneNumber, "+123", "+xxx"},
    {TextAttachment::Type::kPhoneNumber, "+12", "+xx"},
    {TextAttachment::Type::kPhoneNumber, "+1", "+x"},
    {TextAttachment::Type::kPhoneNumber,
     "1255555555555555555555555555555555555555555555555556789",
     "12xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6789"},
    {TextAttachment::Type::kPhoneNumber, "12345678901", "12xxxxx8901"},
    {TextAttachment::Type::kPhoneNumber, "1234567891", "12xxxx7891"},
    {TextAttachment::Type::kPhoneNumber, "123456789", "12xxxx789"},
    {TextAttachment::Type::kPhoneNumber, "12345678", "12xxxx78"},
    {TextAttachment::Type::kPhoneNumber, "1234567", "12xxxx7"},
    {TextAttachment::Type::kPhoneNumber, "123456", "12xxxx"},
    {TextAttachment::Type::kPhoneNumber, "12345", "1xxxx"},
    {TextAttachment::Type::kPhoneNumber, "1234", "xxxx"},
    {TextAttachment::Type::kPhoneNumber, "123", "xxx"},
    {TextAttachment::Type::kPhoneNumber, "12", "xx"},
    {TextAttachment::Type::kPhoneNumber, "1", "x"},
    {TextAttachment::Type::kPhoneNumber, "+", "+"},
};

using TextAttachmentTextTitleTest =
    testing::TestWithParam<TextAttachmentTextTitleTestData>;

}  // namespace

TEST_P(TextAttachmentTextTitleTest, TextTitleMatches) {
  TextAttachment attachment(GetParam().type, GetParam().text_body,
                            /*title=*/std::nullopt,
                            /*mime_type=*/std::nullopt);
  EXPECT_EQ(GetParam().expected_text_title, attachment.text_title());
}

INSTANTIATE_TEST_SUITE_P(TextAttachmentTextTitleTest,
                         TextAttachmentTextTitleTest,
                         testing::ValuesIn(kTextAttachmentTextTitleTestData));

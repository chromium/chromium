// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_text.h"

#include <string>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace ash {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::SizeIs;

TEST(ScannerTextTest, AppendsParagraphs) {
  ScannerText text(u"text content");

  text.AppendParagraph();
  text.AppendParagraph();

  EXPECT_THAT(text.paragraphs(), SizeIs(2));
}

TEST(ScannerTextTest, AppendsLinesToParagraph) {
  ScannerText text(u"text\ncontent");
  ScannerText::Paragraph& paragraph = text.AppendParagraph();

  const ScannerText::CenterRotatedBox kLine1BoundingBox = {
      .center = gfx::Point(10, 10), .size = gfx::Size(20, 8)};
  constexpr gfx::Range kLine1Range(0, 4);
  paragraph.AppendLine(kLine1Range, kLine1BoundingBox);
  const ScannerText::CenterRotatedBox kLine2BoundingBox = {
      .center = gfx::Point(10, 30), .size = gfx::Size(40, 8)};
  constexpr gfx::Range kLine2Range(5, 12);
  paragraph.AppendLine(kLine2Range, kLine2BoundingBox);

  EXPECT_THAT(
      paragraph.lines(),
      ElementsAre(
          AllOf(Property(&ScannerText::Line::range, kLine1Range),
                Property(&ScannerText::Line::bounding_box, kLine1BoundingBox)),
          AllOf(
              Property(&ScannerText::Line::range, kLine2Range),
              Property(&ScannerText::Line::bounding_box, kLine2BoundingBox))));
}

TEST(ScannerTextTest, GetsTextFromRange) {
  ScannerText text(u"abc 😀あ!");

  EXPECT_EQ(text.GetTextFromRange(gfx::Range(0, 3)), u"abc");
  EXPECT_EQ(text.GetTextFromRange(gfx::Range(4, 6)), u"😀");
  EXPECT_EQ(text.GetTextFromRange(gfx::Range(6, 8)), u"あ!");
  EXPECT_EQ(text.GetTextFromRange(gfx::Range(0, 8)), u"abc 😀あ!");
}

TEST(ScannerTextTest, GetsEmptyTextForInvalidRange) {
  ScannerText text(u"abc 😀あ!");

  EXPECT_EQ(text.GetTextFromRange(gfx::Range(0, 9)), u"");
}

}  // namespace

}  // namespace ash

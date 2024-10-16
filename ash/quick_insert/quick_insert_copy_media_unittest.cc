// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_copy_media.h"

#include "ash/public/cpp/system/toast_manager.h"
#include "ash/quick_insert/quick_insert_rich_media.h"
#include "ash/quick_insert/quick_insert_test_util.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

class QuickInsertCopyMediaTest : public AshTestBase {};

TEST_F(QuickInsertCopyMediaTest, CopiesText) {
  CopyMediaToClipboard(PickerTextMedia(u"hello"));

  EXPECT_EQ(ReadTextFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"hello");
}

TEST_F(QuickInsertCopyMediaTest, CopiesImageWithKnownDimensionsAsHtml) {
  CopyMediaToClipboard(
      PickerImageMedia(GURL("https://foo.com"), gfx::Size(30, 20)));

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="https://foo.com/" referrerpolicy="no-referrer" width="30" height="20"/>)html");
}

TEST_F(QuickInsertCopyMediaTest, CopiesImageWithUnknownDimensionsAsHtml) {
  CopyMediaToClipboard(PickerImageMedia(GURL("https://foo.com")));

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="https://foo.com/" referrerpolicy="no-referrer"/>)html");
}

TEST_F(QuickInsertCopyMediaTest,
       CopiesImagesWithBothAltTextAndDimensionsAsHtml) {
  CopyMediaToClipboard(PickerImageMedia(GURL("https://foo.com"),
                                        gfx::Size(30, 20),
                                        /*content_description=*/u"img"));

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="https://foo.com/" referrerpolicy="no-referrer" alt="img" width="30" height="20"/>)html");
}

TEST_F(QuickInsertCopyMediaTest, EscapesAltTextForImages) {
  CopyMediaToClipboard(PickerImageMedia(GURL("https://foo.com"),
                                        /*dimensions=*/std::nullopt,
                                        /*content_description=*/u"\"img\""));

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="https://foo.com/" referrerpolicy="no-referrer" alt="&quot;img&quot;"/>)html");
}

TEST_F(QuickInsertCopyMediaTest, CopiesLinks) {
  CopyMediaToClipboard(PickerLinkMedia(GURL("https://foo.com"), "Foo"));

  EXPECT_EQ(ReadTextFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"https://foo.com/");
  // We include the title as the `title` attribute for maximum compatibility.
  // See `ShouldUseLinkTitle` in picker_insert_media.cc for more details.
  EXPECT_EQ(ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"<a title=\"Foo\" href=\"https://foo.com/\">https://foo.com/</a>");
}

TEST_F(QuickInsertCopyMediaTest, LinksAreEscaped) {
  CopyMediaToClipboard(PickerLinkMedia(
      GURL("https://foo.com/?\"><svg onload=\"alert(1)\"><a href=\""),
      "<svg onload=\"alert(1)\">"));

  EXPECT_EQ(ReadTextFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"https://foo.com/"
            u"?%22%3E%3Csvg%20onload=%22alert(1)%22%3E%3Ca%20href=%22");
  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      u"<a"
      u" title=\"&lt;svg onload=&quot;alert(1)&quot;&gt;\""
      u" href=\"https://foo.com/"
      u"?%22%3E%3Csvg%20onload=%22alert(1)%22%3E%3Ca%20href=%22\">"
      u"https://foo.com/?%22%3E%3Csvg%20onload=%22alert(1)%22%3E%3Ca%20href=%22"
      u"</a>");
}

TEST_F(QuickInsertCopyMediaTest, CopiesFiles) {
  CopyMediaToClipboard(PickerLocalFileMedia(base::FilePath("/foo.txt")));

  EXPECT_EQ(ReadFilenameFromClipboard(ui::Clipboard::GetForCurrentThread()),
            base::FilePath("/foo.txt"));
}

class QuickInsertCopyMediaToastTest
    : public AshTestBase,
      public testing::WithParamInterface<PickerRichMedia> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertCopyMediaToastTest,
    ::testing::Values(PickerTextMedia(u"hello"),
                      PickerImageMedia(GURL("https://foo.com"),
                                       gfx::Size(30, 20)),
                      PickerLinkMedia(GURL("https://foo.com"), "Foo"),
                      PickerLocalFileMedia(base::FilePath("/foo.txt"))));

TEST_P(QuickInsertCopyMediaToastTest, ShowsToastAfterCopyingLink) {
  CopyMediaToClipboard(GetParam());

  EXPECT_TRUE(
      ash::ToastManager::Get()->IsToastShown("picker_copy_to_clipboard"));
}

}  // namespace
}  // namespace ash

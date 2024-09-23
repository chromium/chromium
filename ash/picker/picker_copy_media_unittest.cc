// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_copy_media.h"

#include "ash/picker/picker_rich_media.h"
#include "ash/picker/picker_test_util.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

namespace ash {
namespace {

class PickerCopyMediaTest : public AshTestBase {};

TEST_F(PickerCopyMediaTest, CopiesText) {
  CopyMediaToClipboard(PickerTextMedia(u"hello"));

  EXPECT_EQ(ReadTextFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"hello");
}

TEST_F(PickerCopyMediaTest, CopiesLinks) {
  CopyMediaToClipboard(PickerLinkMedia(GURL("https://foo.com"), "Foo"));

  EXPECT_EQ(ReadTextFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"https://foo.com/");
  // We include the title as the `title` attribute for maximum compatibility.
  // See `ShouldUseLinkTitle` in picker_insert_media.cc for more details.
  EXPECT_EQ(ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
            u"<a title=\"Foo\" href=\"https://foo.com/\">https://foo.com/</a>");
}

TEST_F(PickerCopyMediaTest, LinksAreEscaped) {
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

TEST_F(PickerCopyMediaTest, CopiesFiles) {
  CopyMediaToClipboard(PickerLocalFileMedia(base::FilePath("/foo.txt")));

  EXPECT_EQ(ReadFilenameFromClipboard(ui::Clipboard::GetForCurrentThread()),
            base::FilePath("/foo.txt"));
}

class PickerCopyMediaToastTest
    : public AshTestBase,
      public testing::WithParamInterface<PickerRichMedia> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerCopyMediaToastTest,
    ::testing::Values(PickerTextMedia(u"hello"),
                      PickerLinkMedia(GURL("https://foo.com"), "Foo"),
                      PickerLocalFileMedia(base::FilePath("/foo.txt"))));

TEST_P(PickerCopyMediaToastTest, ShowsToastAfterCopyingLink) {
  CopyMediaToClipboard(GetParam());

  EXPECT_TRUE(
      ash::ToastManager::Get()->IsToastShown("picker_copy_to_clipboard"));
}

}  // namespace
}  // namespace ash

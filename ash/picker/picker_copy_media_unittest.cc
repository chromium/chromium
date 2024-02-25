// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_copy_media.h"

#include "ash/picker/picker_test_util.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

namespace ash {
namespace {

class PickerCopyMediaTest : public AshTestBase {};

TEST_F(PickerCopyMediaTest, CopiesGifAsHtml) {
  CopyGifMediaToClipboard(GURL("https://foo.com"),
                          /*content_description=*/u"a gif");

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="https://foo.com/" referrerpolicy="no-referrer" alt="a gif"/>)html");
}

TEST_F(PickerCopyMediaTest, EscapesAltText) {
  CopyGifMediaToClipboard(GURL("https://foo.com"),
                          /*content_description=*/u"a \"gif\"");

  EXPECT_EQ(
      ReadHtmlFromClipboard(ui::Clipboard::GetForCurrentThread()),
      uR"html(<img src="https://foo.com/" referrerpolicy="no-referrer" alt="a &quot;gif&quot;"/>)html");
}

TEST_F(PickerCopyMediaTest, ShowsToast) {
  CopyGifMediaToClipboard(GURL("https://foo.com"),
                          /*content_description=*/u"a gif");

  EXPECT_TRUE(
      ash::ToastManager::Get()->IsToastShown("picker_copy_to_clipboard"));
}

}  // namespace
}  // namespace ash

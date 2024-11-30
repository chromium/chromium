// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
TEST(WebAuthFlowInfoBarDelegateTest, LongExtensionName) {
  const std::string long_extension_name =
      "This is a very very very very very very long extension name that should "
      "be truncated";
  const std::u16string truncated_extension_name =
      util::GetFixupExtensionNameForUIDisplay(long_extension_name);
  ASSERT_LT(truncated_extension_name.size(), long_extension_name.size());

  const WebAuthFlowInfoBarDelegate delegate(long_extension_name);
  const std::u16string infobar_text = delegate.GetMessageText();

  EXPECT_FALSE(
      base::Contains(infobar_text, base::UTF8ToUTF16(long_extension_name)));
  EXPECT_TRUE(base::Contains(infobar_text, truncated_extension_name));
}
}  // namespace extensions

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include "chrome/browser/themes/theme_properties.h"
#include "extensions/common/image_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// This test verifies that the assumed default color of the toolbar
// doesn't change, and if it does, we update the default value. We
// need this test at the browser level, since the lower levels where
// we use this value don't have access to the ThemeService.
//
// TODO(pkasting): The validation that uses this color should happen at some
// point where the requesting Chrome window can supply the relevant toolbar
// color through an interface of some sort, removing this hardcoded value.
TEST(ExtensionUiUtilTest, CheckDefaultToolbarColor) {
  EXPECT_EQ(
      image_util::kDefaultToolbarColor,
      ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_TOOLBAR, false))
      << "Please update image_util::kDefaultToolbarColor to the new value";
}

}  // namespace extensions

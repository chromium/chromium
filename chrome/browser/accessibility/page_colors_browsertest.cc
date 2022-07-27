// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/native_theme/native_theme.h"

class PageColorsBrowserTest : public InProcessBrowserTest {
 public:
  PageColorsBrowserTest() = default;

  PageColorsBrowserTest(const PageColorsBrowserTest&) = delete;
  PageColorsBrowserTest& operator=(const PageColorsBrowserTest&) = delete;
};

// Changing the kPageColors pref should affect the state of Page Colors in the
// NativeTheme.
IN_PROC_BROWSER_TEST_F(PageColorsBrowserTest, PageColorsPrefChange) {
  ui::NativeTheme::PageColors page_colors_pref =
      static_cast<ui::NativeTheme::PageColors>(
          browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::NativeTheme::PageColors page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_state, page_colors_pref);

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kDusk);
  page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_state, ui::NativeTheme::PageColors::kDusk);
}

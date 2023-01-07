// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_common_utils.h"

#include <memory>

#include "chrome/common/webui_url_constants.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AppendNavigations(sessions::SessionTab* tab,
                       content::NavigationEntry* entry) {
  tab->navigations.push_back(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(0,
                                                                        entry));
}

}  // namespace

class SessionCommonUtilTest : public ::testing::Test {};

TEST_F(SessionCommonUtilTest, GetSelectedIndex) {
  GURL::Replacements replace_sign_out_path;
  replace_sign_out_path.SetPathStr(chrome::kSignOutSubPage);
  const GURL settings_page(chrome::kChromeUISettingsURL);
  const GURL sign_out_page(
      settings_page.ReplaceComponents(replace_sign_out_path));
  sessions::SessionTab tab;

  std::unique_ptr<content::NavigationEntry> entry1(
      content::NavigationEntry::Create());
  std::unique_ptr<content::NavigationEntry> entry2(
      content::NavigationEntry::Create());
  entry1->SetVirtualURL(settings_page);
  entry2->SetVirtualURL(sign_out_page);

  AppendNavigations(&tab, entry1.get());
  AppendNavigations(&tab, entry2.get());

  tab.current_navigation_index = 0;
  ASSERT_EQ(0, GetNavigationIndexToSelect(tab));
  tab.current_navigation_index = 1;
  ASSERT_EQ(0, GetNavigationIndexToSelect(tab));
}

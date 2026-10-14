// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = tabs_constants;
namespace utils = api_test_utils;

using ExtensionTabsApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ExtensionTabsApiTest, QueryLastFocusedWindowTabs) {
  const size_t kExtraWindows = 2;
  for (size_t i = 0; i < kExtraWindows; ++i) {
    CreateBrowser(GetProfile());
  }

  BrowserWindowInterface* focused_window = CreateBrowser(GetProfile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(focused_window));

  GURL url("about:blank");
  ASSERT_TRUE(AddTabAtIndexToBrowser(focused_window, 0, url,
                                     ui::PAGE_TRANSITION_LINK, true));
  int focused_window_id =
      extensions::ExtensionTabUtil::GetWindowId(focused_window);

  // Get tabs in the 'last focused' window called from non-focused browser.
  scoped_refptr<extensions::TabsQueryFunction> function =
      new extensions::TabsQueryFunction();
  scoped_refptr<const extensions::Extension> extension(
      extensions::ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::ListValue result_tabs(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"lastFocusedWindow\":true}]", GetProfile())));

  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    EXPECT_EQ(focused_window_id,
              api_test_utils::GetInteger(utils::ToDict(result_tab),
                                         keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'last focused' window called from the focused browser.
  function = new extensions::TabsQueryFunction();
  function->set_extension(extension.get());
  result_tabs = utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"lastFocusedWindow\":false}]", GetProfile()));

  // We should get one tab for each extra window and one for the initial window.
  EXPECT_EQ(kExtraWindows + 1, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    EXPECT_NE(focused_window_id,
              api_test_utils::GetInteger(utils::ToDict(result_tab),
                                         keys::kWindowIdKey));
  }
}

}  // namespace extensions

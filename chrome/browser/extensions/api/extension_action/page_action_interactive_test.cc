// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {
namespace {

using PageActionInteractiveTest = ExtensionApiTest;

// Tests popups in page actions.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveTest, ShowPageActionPopup) {
  ASSERT_TRUE(RunExtensionTest("page_action/popup")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(browser()->window()->IsActive());

  ResultCatcher catcher;
  ExtensionActionTestHelper::Create(browser())->Press(extension->id());
  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace
}  // namespace extensions

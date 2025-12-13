// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt_show_params.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/test/test_windows.h"

using ExtensionInstallPromptShowParamsTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptShowParamsTest,
                       WasParentDestroyedOutsideOfRoot) {
  aura::Window* context_window = browser()->window()->GetNativeWindow();

  ExtensionInstallPromptShowParams params(GetProfile(), context_window);
  ASSERT_TRUE(context_window->GetRootWindow());
  // As the context window is parented to a root, the parent is valid.
  EXPECT_FALSE(params.WasParentDestroyed());

  std::unique_ptr<aura::Window> window_with_no_root_ancestor =
      aura::test::CreateTestWindow({.bounds = {100, 100}, .window_id = 11});
  ExtensionInstallPromptShowParams params2(GetProfile(),
                                           window_with_no_root_ancestor.get());
  // As `window_with_no_root_ancestor` is not parented to a root, it should
  // return true from WasParentDestroyed().
  EXPECT_TRUE(params2.WasParentDestroyed());
}

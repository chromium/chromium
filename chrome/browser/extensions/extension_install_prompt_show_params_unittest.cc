// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt_show_params.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/aura/test/test_windows.h"

using ExtensionInstallPromptShowParamsTest = BrowserWithTestWindowTest;

TEST_F(ExtensionInstallPromptShowParamsTest, WasParentDestroyedOutsideOfRoot) {
  ExtensionInstallPromptShowParams params(profile(), GetContext());
  ASSERT_TRUE(GetContext()->GetRootWindow());
  // As the context window is parented to a root, the parent is valid.
  EXPECT_FALSE(params.WasParentDestroyed());

  std::unique_ptr<aura::Window> window_with_no_root_ancestor(
      aura::test::CreateTestWindowWithId(11, nullptr));
  ExtensionInstallPromptShowParams params2(profile(),
                                           window_with_no_root_ancestor.get());
  // As `window_with_no_root_ancestor` is not parented to a root, it should
  // return true from WasParentDestroyed().
  EXPECT_TRUE(params2.WasParentDestroyed());
}

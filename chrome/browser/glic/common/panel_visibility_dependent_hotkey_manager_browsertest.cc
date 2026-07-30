// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class PanelVisibilityDependentHotkeyManagerBrowserTest
    : public GlicBrowserTest {};

IN_PROC_BROWSER_TEST_F(PanelVisibilityDependentHotkeyManagerBrowserTest,
                       FocusToggleTogglesFocus) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  // Glic should have focus.
  ASSERT_OK(FocusGlic(instance));

  // Press focus toggle hotkey to toggle focus back to browser.
  TriggerHotkey(LocalHotkeyManager::Command::kFocusToggle);

  // Verify Glic lost focus.
  ASSERT_TRUE(RunUntilEqual<bool>(
      [&]() { return instance->GetActiveEmbedder()->HasFocus(); }, false,
      "Timeout waiting for Glic to lose focus"));

  // Press focus toggle hotkey again to focus Glic.
  TriggerHotkey(LocalHotkeyManager::Command::kFocusToggle);

  // Verify Glic got focus.
  ASSERT_TRUE(RunUntilEqual<bool>(
      [&]() { return instance->GetActiveEmbedder()->HasFocus(); }, true,
      "Timeout waiting for Glic to get focus"));
}

}  // namespace
}  // namespace glic

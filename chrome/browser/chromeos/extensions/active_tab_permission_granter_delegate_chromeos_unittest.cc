// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/active_tab_permission_granter_delegate_chromeos.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chromeos/login/scoped_test_public_session_login_state.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
// Use an extension ID that will never be whitelisted.
const char kNonWhitelistedId[] = "bogus";

scoped_refptr<const Extension> CreateExtension(const std::string& id) {
  return ExtensionBuilder("test").SetID(id).Build();
}

}  // namespace

class ActiveTabPermissionGranterDelegateChromeOSTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override;
  void TearDown() override;

  ActiveTabPermissionGranterDelegateChromeOS delegate_;
  std::unique_ptr<chromeos::ScopedTestPublicSessionLoginState> login_state_;
};

void ActiveTabPermissionGranterDelegateChromeOSTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  login_state_.reset(new chromeos::ScopedTestPublicSessionLoginState());
}

void ActiveTabPermissionGranterDelegateChromeOSTest::TearDown() {
  login_state_.reset();
  permission_helper::ResetPermissionsForTesting();
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(ActiveTabPermissionGranterDelegateChromeOSTest, GrantedForWhitelisted) {
  auto extension = CreateExtension(kWhitelistedId);
  EXPECT_TRUE(delegate_.ShouldGrantActiveTabOrPrompt(extension.get(), nullptr));
}

TEST_F(ActiveTabPermissionGranterDelegateChromeOSTest,
       RejectedForNonWhitelisted) {
  auto extension = CreateExtension(kNonWhitelistedId);
  // Deny the permission request.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::CANCEL);
  // First request is always rejected (by design).
  EXPECT_FALSE(
      delegate_.ShouldGrantActiveTabOrPrompt(extension.get(), nullptr));
  // Spin the loop, allowing the dialog to be resolved.
  base::RunLoop().RunUntilIdle();
  // Dialog result is propagated here, permission request is rejected.
  EXPECT_FALSE(
      delegate_.ShouldGrantActiveTabOrPrompt(extension.get(), nullptr));
}

TEST_F(ActiveTabPermissionGranterDelegateChromeOSTest,
       GrantedForNonWhitelisted) {
  auto extension = CreateExtension(kNonWhitelistedId);
  // Allow the permission request.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  EXPECT_FALSE(
      delegate_.ShouldGrantActiveTabOrPrompt(extension.get(), nullptr));
  base::RunLoop().RunUntilIdle();
  // The permission request is granted now.
  EXPECT_TRUE(delegate_.ShouldGrantActiveTabOrPrompt(extension.get(), nullptr));
}

}  // namespace extensions

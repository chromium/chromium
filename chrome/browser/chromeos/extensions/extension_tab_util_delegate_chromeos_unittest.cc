// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"

#include <string>

#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
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

class ExtensionTabUtilDelegateChromeOSTest : public testing::Test {
 protected:
  ExtensionTabUtilDelegateChromeOS delegate_;
};

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       NoFilteringOutsidePublicSessionForWhitelisted) {
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  auto extension = CreateExtension(kWhitelistedId);
  EXPECT_EQ(delegate_.GetScrubTabBehavior(extension.get()),
            ExtensionTabUtil::kDontScrubTab);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       NoFilteringOutsidePublicSessionForNonWhitelisted) {
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  auto extension = CreateExtension(kNonWhitelistedId);
  EXPECT_EQ(delegate_.GetScrubTabBehavior(extension.get()),
            ExtensionTabUtil::kDontScrubTab);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       NoFilteringInsidePublicSessionForWhitelisted) {
  chromeos::ScopedTestPublicSessionLoginState state;

  auto extension = CreateExtension(kWhitelistedId);
  EXPECT_EQ(delegate_.GetScrubTabBehavior(extension.get()),
            ExtensionTabUtil::kDontScrubTab);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       FilterInsidePublicSessionNonWhitelisted) {
  chromeos::ScopedTestPublicSessionLoginState state;

  auto extension = CreateExtension(kNonWhitelistedId);
  EXPECT_EQ(delegate_.GetScrubTabBehavior(extension.get()),
            ExtensionTabUtil::kScrubTabUrlToOrigin);
}

}  // namespace extensions

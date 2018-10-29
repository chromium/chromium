// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"

#include <string>

#include "chromeos/login/login_state.h"
#include "chromeos/login/scoped_test_public_session_login_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
// Use an extension ID that will never be whitelisted.
const char kNonWhitelistedId[] = "bogus";

const char kTestUrl[] = "http://www.foo.bar/baz?key=val";
const char kFilteredUrl[] = "http://www.foo.bar/";

scoped_refptr<const Extension> CreateExtension(const std::string& id) {
  return ExtensionBuilder("test").SetID(id).Build();
}

}  // namespace

class ExtensionTabUtilDelegateChromeOSTest : public testing::Test {
 protected:
  void SetUp() override;

  ExtensionTabUtilDelegateChromeOS delegate_;
  api::tabs::Tab tab_;
};

void ExtensionTabUtilDelegateChromeOSTest::SetUp() {
  tab_.url.reset(new std::string(kTestUrl));
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       NoFilteringOutsidePublicSessionForWhitelisted) {
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  auto extension = CreateExtension(kWhitelistedId);
  delegate_.ScrubTabForExtension(extension.get(), nullptr, &tab_);
  EXPECT_EQ(kTestUrl, *tab_.url);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       NoFilteringOutsidePublicSessionForNonWhitelisted) {
  ASSERT_FALSE(chromeos::LoginState::IsInitialized());

  auto extension = CreateExtension(kNonWhitelistedId);
  delegate_.ScrubTabForExtension(extension.get(), nullptr, &tab_);
  EXPECT_EQ(kTestUrl, *tab_.url);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       NoFilteringInsidePublicSessionForWhitelisted) {
  chromeos::ScopedTestPublicSessionLoginState state;

  auto extension = CreateExtension(kWhitelistedId);
  delegate_.ScrubTabForExtension(extension.get(), nullptr, &tab_);
  EXPECT_EQ(kTestUrl, *tab_.url);
}

TEST_F(ExtensionTabUtilDelegateChromeOSTest,
       FilterInsidePublicSessionNonWhitelisted) {
  chromeos::ScopedTestPublicSessionLoginState state;

  auto extension = CreateExtension(kNonWhitelistedId);
  delegate_.ScrubTabForExtension(extension.get(), nullptr, &tab_);
  EXPECT_EQ(kFilteredUrl, *tab_.url);
}

}  // namespace extensions

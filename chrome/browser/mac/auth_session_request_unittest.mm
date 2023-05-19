// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/auth_session_request.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TEST(AuthSessionRequestTest, SchemeCanonicalization) {
  if (@available(macOS 10.15, *)) {
    EXPECT_EQ("abcdefg", AuthSessionRequest::CanonicalizeScheme("abcdefg"));
    EXPECT_EQ("abcdefg", AuthSessionRequest::CanonicalizeScheme("aBcDeFg"));
    EXPECT_EQ(absl::nullopt, AuthSessionRequest::CanonicalizeScheme("🥰"));
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

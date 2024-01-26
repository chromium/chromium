// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/auth_session_request.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(AuthSessionRequestTest, SchemeCanonicalization) {
  EXPECT_EQ("abcdefg", AuthSessionRequest::CanonicalizeScheme("abcdefg"));
  EXPECT_EQ("abcdefg", AuthSessionRequest::CanonicalizeScheme("aBcDeFg"));
  EXPECT_EQ(std::nullopt, AuthSessionRequest::CanonicalizeScheme("🥰"));
}

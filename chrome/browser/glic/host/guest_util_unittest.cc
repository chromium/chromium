// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

TEST(GuestUtilTest, GetLocalizedGuestURLAddsLanguageParameter) {
  EXPECT_EQ(GURL("https://www.google.com?hl=en"),
            GetLocalizedGuestURL(GURL("https://www.google.com")));
}

TEST(GuestUtilTest, GetLocalizedGuestURLDoesNotChangeLanguageParameter) {
  EXPECT_EQ(GURL("https://www.google.com?hl=es"),
            GetLocalizedGuestURL(GURL("https://www.google.com?hl=es")));
}

}  // namespace

}  // namespace glic

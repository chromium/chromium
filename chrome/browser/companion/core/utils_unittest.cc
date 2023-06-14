// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/utils.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {

namespace {

class CompanionCoreUtilsTest : public testing::Test {};

TEST_F(CompanionCoreUtilsTest, HomepageURLForCompanion) {
  EXPECT_EQ("https://lens.google.com/companion", GetHomepageURLForCompanion());
}

TEST_F(CompanionCoreUtilsTest, ImageUploadURLForCompanion) {
  EXPECT_EQ("https://lens.google.com/upload", GetImageUploadURLForCompanion());
}

}  // namespace

}  // namespace companion

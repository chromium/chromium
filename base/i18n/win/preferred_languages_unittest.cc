// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for Windows internationalization funcs.

#include "base/i18n/win/preferred_languages.h"

#include <stddef.h>
#include <string.h>

#include "base/compiler_specific.h"
#include "base/i18n/language_tag.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

// Tests that at least one user preferred UI language can be obtained.
TEST(I18NTest, GetUserPreferredUILanguageList) {
  const std::vector<LanguageTag> languages = GetUserPreferredUILanguageList();
  EXPECT_FALSE(languages.empty());
  for (const auto& language : languages) {
    EXPECT_FALSE(language.tag_string().empty());
  }
}

// Tests that at least one thread preferred UI language can be obtained.
TEST(I18NTest, GetThreadPreferredUILanguageList) {
  const std::vector<LanguageTag> languages = GetThreadPreferredUILanguageList();
  EXPECT_FALSE(languages.empty());
  for (const auto& language : languages) {
    EXPECT_FALSE(language.tag_string().empty());
  }
}

}  // namespace base::i18n

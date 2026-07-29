// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/test/test_locale_holder.h"

#include "base/i18n/language_tag.h"
#include "base/i18n/locale_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {
namespace {

TEST(ScopedLocaleHelpersTest, ScopedLocaleOverride) {
  ThreadSafeLocaleHolder holder(GetKnownLanguageTag("en-US"));
  EXPECT_EQ(holder.GetLocale(), GetKnownLanguageTag("en-US"));

  {
    ScopedLocaleOverride<ThreadSafeLocaleHolder> scoped_override(
        holder, GetKnownLanguageTag("fr-FR"));
    EXPECT_EQ(holder.GetLocale(), GetKnownLanguageTag("fr-FR"));
  }

  // Expect original locale to be restored.
  EXPECT_EQ(holder.GetLocale(), GetKnownLanguageTag("en-US"));
}

}  // namespace
}  // namespace base::i18n

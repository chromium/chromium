// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/timezone.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/strenum.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
namespace {

TEST(TimezoneTest, CountryCodeForTimezones) {
  std::unique_ptr<icu::StringEnumeration> timezones(
      icu::TimeZone::createEnumeration());

  UErrorCode status = U_ZERO_ERROR;
  while (const icu::UnicodeString* timezone = timezones->snext(status)) {
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(*timezone));

    std::string country_code = CountryCodeForCurrentTimezone();
    // On some systems (such as Android or some flavors of Linux), ICU may come
    // up empty. With https://chromium-review.googlesource.com/c/512282/ , ICU
    // will not fail any more. See also
    // http://bugs.icu-project.org/trac/ticket/13208 . Even with that, ICU
    // returns '001' (world) for region-agnostic timezones such as Etc/UTC and
    // |CountryCodeForCurrentTimezone| returns an empty string so that the next
    // fallback can be tried by a customer.
    if (!country_code.empty())
      EXPECT_EQ(2U, country_code.size()) << "country_code = " << country_code;
  }

  icu::TimeZone::adoptDefault(nullptr);
}

}  // namespace
}  // namespace base

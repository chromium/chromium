// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class L10nUtilBrowserTest : public InProcessBrowserTest {
 public:
  L10nUtilBrowserTest() = default;
  ~L10nUtilBrowserTest() override = default;
  L10nUtilBrowserTest(const L10nUtilBrowserTest&) = delete;
  L10nUtilBrowserTest& operator=(const L10nUtilBrowserTest&) = delete;
};

}  // namespace

// Tests whether CheckAndResolveLocale returns the same result with and without
// I/O.
IN_PROC_BROWSER_TEST_F(L10nUtilBrowserTest, CheckAndResolveLocaleIO) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::vector<std::string> accept_languages;
  l10n_util::GetAcceptLanguages(&accept_languages);

  for (const std::string& locale : accept_languages) {
    std::string resolved_locale;
    bool resolved = l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                                     /*perform_io=*/false);
    std::string resolved_locale_with_io;
    bool resolved_with_io = l10n_util::CheckAndResolveLocale(
        locale, &resolved_locale_with_io, /*perform_io=*/true);

#if BUILDFLAG(IS_ANDROID)
    // False positives may occur on Android and iOS (and chrome/ isn't used on
    // iOS, so we only need to check for Android).
    // False negatives should never occur - so if the call without IO returns
    // false, the call with I/O must return false too.
    if (!resolved) {
      EXPECT_FALSE(resolved_with_io)
          << "Couldn't resolve " << locale
          << " without IO, but resolving with IO successfully returned "
          << resolved_locale_with_io;
    }
    // If CheckAndResolveLocale returns the same locale as the input, that means
    // that we have strings for that locale. False negatives should never occur
    // like this as well - if the call without I/O returns something different
    // to the input, the same should apply to the call with I/O.
    if (resolved_locale != locale) {
      EXPECT_NE(resolved_locale_with_io, locale)
          << "Resolving " << locale
          << " without IO returned a different locale ("
          << (resolved_locale.empty() ? "an empty string" : resolved_locale)
          << "), but resolving with IO returned the same locale";
    }
#else
    // On other platforms, the two function calls should be identical.
    EXPECT_EQ(resolved, resolved_with_io);
    EXPECT_EQ(resolved_locale, resolved_locale_with_io);
#endif
  }
}

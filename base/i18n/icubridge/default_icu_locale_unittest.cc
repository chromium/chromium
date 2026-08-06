// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/default_icu_locale.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {

TEST(DefaultIcuLocaleTest, SetAndGetLocale) {
  LanguageTag pt_br = GetKnownLanguageTag("pt-BR");
  ScopedDefaultIcuLocale override_locale(pt_br);

  LanguageTag retrieved = GetDefaultIcuLocale();
  EXPECT_EQ(retrieved, pt_br);
}

TEST(DefaultIcuLocaleTest, UpdateLocale) {
  LanguageTag pt_br = GetKnownLanguageTag("pt-BR");
  ScopedDefaultIcuLocale override_pt(pt_br);
  LanguageTag retrieved = GetDefaultIcuLocale();
  EXPECT_EQ(retrieved, pt_br);

  {
    LanguageTag fr_fr = GetKnownLanguageTag("fr-FR");
    ScopedDefaultIcuLocale override_fr(fr_fr);
    retrieved = GetDefaultIcuLocale();
    EXPECT_EQ(retrieved, fr_fr);
  }

  retrieved = GetDefaultIcuLocale();
  EXPECT_EQ(retrieved, pt_br);
}

TEST(DefaultIcuLocaleTest, SyncsWithIcuDefaultLocale) {
  LanguageTag pt_br = GetKnownLanguageTag("pt-BR");
  ScopedDefaultIcuLocale override_pt(pt_br);

  // Verify that the ICU library's default locale was updated.
  EXPECT_STREQ(icu::Locale::getDefault().getName(), "pt_BR");
}

TEST(DefaultIcuLocaleTest, ThreadSafety) {
  base::test::TaskEnvironment task_environment;

  // Run multiple reader and writer tasks concurrently on the ThreadPool.
  constexpr int kNumTasks = 20;
  base::RunLoop run_loop;
  auto barrier_closure =
      base::BarrierClosure(kNumTasks, run_loop.QuitClosure());

  for (int i = 0; i < kNumTasks; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, {},
        base::BindOnce(
            [](int idx, base::RepeatingClosure done) {
              if (idx % 2 == 0) {
                LanguageTag pt_br = GetKnownLanguageTag("pt-BR");
                ScopedDefaultIcuLocale override_locale(pt_br);
                LanguageTag current = GetDefaultIcuLocale();
                EXPECT_TRUE(current == GetKnownLanguageTag("pt-BR") ||
                            current == GetKnownLanguageTag("fr-FR") ||
                            current == GetKnownLanguageTag("en-US"));
              } else {
                LanguageTag fr_fr = GetKnownLanguageTag("fr-FR");
                ScopedDefaultIcuLocale override_locale(fr_fr);
                LanguageTag current = GetDefaultIcuLocale();
                EXPECT_TRUE(current == GetKnownLanguageTag("pt-BR") ||
                            current == GetKnownLanguageTag("fr-FR") ||
                            current == GetKnownLanguageTag("en-US"));
              }
              std::move(done).Run();
            },
            i, barrier_closure));
  }
  run_loop.Run();
}

}  // namespace base::i18n

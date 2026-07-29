// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TEST_TEST_LOCALE_HOLDER_H_
#define BASE_I18N_TEST_TEST_LOCALE_HOLDER_H_

#include <type_traits>

#include "base/check_is_test.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/locale_holder.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"

namespace base::i18n {

// ScopedLocaleOverride is a test helper that temporarily overrides the locale
// of a specified ThreadSafeLocaleHolder during its lifetime, restoring the
// original value on destruction.
template <typename T>
class ScopedLocaleOverride {
  static_assert(std::is_base_of_v<ThreadSafeLocaleHolder, T> ||
                    std::is_base_of_v<SequenceCheckedLocaleHolder, T>,
                "ScopedLocaleOverride can only be used with subclasses of "
                "ThreadSafeLocaleHolder");

 public:
  ScopedLocaleOverride(T& holder, const LanguageTag& locale)
      : holder_(holder), original_locale_(holder.GetLocale()) {
    CHECK_IS_TEST();
    holder.SetLocale(locale);
  }

  ~ScopedLocaleOverride() { holder_->SetLocale(original_locale_); }

  ScopedLocaleOverride(const ScopedLocaleOverride&) = delete;
  ScopedLocaleOverride& operator=(const ScopedLocaleOverride&) = delete;

 private:
  const raw_ref<T> holder_;
  LanguageTag original_locale_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_TEST_TEST_LOCALE_HOLDER_H_

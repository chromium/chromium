// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TEST_SCOPED_ICU_LOCALE_H_
#define BASE_I18N_TEST_SCOPED_ICU_LOCALE_H_

#include "base/i18n/language_tag.h"

namespace base::i18n {

// ScopedDefaultIcuLocale is a test helper that sets the global default ICU
// locale to a test value during its lifetime, and automatically restores the
// original locale upon destruction.
class ScopedDefaultIcuLocale {
 public:
  explicit ScopedDefaultIcuLocale(const LanguageTag& locale);
  ~ScopedDefaultIcuLocale();

  ScopedDefaultIcuLocale(const ScopedDefaultIcuLocale&) = delete;
  ScopedDefaultIcuLocale& operator=(const ScopedDefaultIcuLocale&) = delete;

 private:
  LanguageTag original_locale_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_TEST_SCOPED_ICU_LOCALE_H_

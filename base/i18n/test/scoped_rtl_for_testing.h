// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TEST_SCOPED_RTL_FOR_TESTING_H_
#define BASE_I18N_TEST_SCOPED_RTL_FOR_TESTING_H_

#include <memory>

#include "base/i18n/base_i18n_export.h"

namespace base::i18n {

class ScopedDefaultIcuLocale;

// A RAII wrapper for setting RTL in tests. Automatically restores the previous
// RTL state when destroyed. This is the preferred way to set RTL state in
// tests.
class ScopedRTLForTesting {
 public:
  explicit ScopedRTLForTesting(bool rtl);
  ~ScopedRTLForTesting();

  // Not copyable or movable
  ScopedRTLForTesting(const ScopedRTLForTesting&) = delete;
  ScopedRTLForTesting& operator=(const ScopedRTLForTesting&) = delete;

 private:
  bool previous_rtl_state_;
  std::unique_ptr<ScopedDefaultIcuLocale> scoped_locale_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_TEST_SCOPED_RTL_FOR_TESTING_H_

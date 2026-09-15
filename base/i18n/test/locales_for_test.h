// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TEST_LOCALES_FOR_TEST_H_
#define BASE_I18N_TEST_LOCALES_FOR_TEST_H_

#include "base/i18n/language_tag.h"

namespace base::i18n {

// Returns a comprehensive list of locales to be used in tests.
std::vector<LanguageTag> GetLocalesForTest();

}  // namespace base::i18n

#endif  // BASE_I18N_TEST_LOCALES_FOR_TEST_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/test/scoped_rtl_for_testing.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_icu_locale.h"

namespace base::i18n {

ScopedRTLForTesting::ScopedRTLForTesting(bool rtl)
    : previous_rtl_state_(IsRTL()),
      scoped_locale_(std::make_unique<ScopedDefaultIcuLocale>(
          rtl ? GetKnownLanguageTag("he") : GetKnownLanguageTag("en"))) {
  CHECK_IS_TEST();
  CHECK_EQ(rtl, IsRTL());
}

ScopedRTLForTesting::~ScopedRTLForTesting() {
  scoped_locale_.reset();
  DCHECK_EQ(previous_rtl_state_, IsRTL());
}

}  // namespace base::i18n

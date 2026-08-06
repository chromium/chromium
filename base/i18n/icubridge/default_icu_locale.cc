// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/default_icu_locale.h"

#include "base/i18n/language_tag.h"
#include "base/i18n/locale_holder.h"
#include "base/i18n/tag_converters.h"
#include "base/no_destructor.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {

namespace {

ThreadSafeLocaleHolder& GetLocaleHolder() {
  static base::NoDestructor<ThreadSafeLocaleHolder> holder(
      GetKnownLanguageTag("en-US"));
  return *holder;
}

}  // namespace

LanguageTag GetDefaultIcuLocale() {
  return GetLocaleHolder().GetLocale();
}

void SetDefaultIcuLocale(DefaultIcuLocaleSetterKey key,
                         const LanguageTag& language_tag) {
  GetLocaleHolder().SetLocale(language_tag);
  {
    UErrorCode error_code = U_ZERO_ERROR;
    icu::Locale locale =
        IcuLocaleConverter::GetInstance().FromLanguageTag(language_tag);
    icu::Locale::setDefault(locale, error_code);
  }
}

}  // namespace base::i18n

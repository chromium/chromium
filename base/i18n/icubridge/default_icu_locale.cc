// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/default_icu_locale.h"

#include "base/i18n/icu4c_tag_converter.h"  // nogncheck
#include "base/i18n/language_tag.h"
#include "base/i18n/locale_holder.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/locid.h"

#if BUILDFLAG(IS_IOS)
#include "base/debug/crash_logging.h"
#include "base/ios/ios_util.h"
#endif

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
#if BUILDFLAG(IS_IOS)
  static base::debug::CrashKeyString* crash_key_locale =
      base::debug::AllocateCrashKeyString("icu_locale_input",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(crash_key_locale, language_tag.tag_string());
#endif
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale locale =
      IcuLocaleConverter::GetInstance().FromLanguageTag(language_tag);
  icu::Locale::setDefault(locale, error_code);
  if (U_FAILURE(error_code)) {
    LOG(ERROR) << "Failed to set the ICU default locale to " << language_tag
               << ". Falling back to en-US.";
    icu::Locale::setDefault(icu::Locale::getUS(), error_code);
    GetLocaleHolder().SetLocale(GetKnownLanguageTag("en-US"));
  }
}

}  // namespace base::i18n

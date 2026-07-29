// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/locale_holder.h"

#include "base/i18n/language_tag.h"

namespace base::i18n {

using ::base::i18n::LanguageTag;

ThreadSafeLocaleHolder::ThreadSafeLocaleHolder(LanguageTag initial_locale)
    : locale_(initial_locale) {}

ThreadSafeLocaleHolder::~ThreadSafeLocaleHolder() = default;

LanguageTag ThreadSafeLocaleHolder::GetLocale() const {
  base::AutoLock lock(lock_);
  return locale_;
}

void ThreadSafeLocaleHolder::SetLocale(const LanguageTag& locale) {
  base::AutoLock lock(lock_);
  if (locale_ != locale) {
    locale_ = locale;
  }
}

SequenceCheckedLocaleHolder::SequenceCheckedLocaleHolder(
    LanguageTag initial_locale)
    : holder_(initial_locale) {}

SequenceCheckedLocaleHolder::~SequenceCheckedLocaleHolder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

LanguageTag SequenceCheckedLocaleHolder::GetLocale() const {
  return holder_.GetLocale();
}

void SequenceCheckedLocaleHolder::SetLocale(const LanguageTag& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  holder_.SetLocale(locale);
}

}  // namespace base::i18n

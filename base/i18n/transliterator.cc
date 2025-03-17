// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/transliterator.h"

#include <stdint.h>

#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace base {
namespace i18n {

class ICUTransliterator : public Transliterator {
 public:
  ICUTransliterator(icu::Transliterator* instance) : delegate_(instance) {}
  ~ICUTransliterator() override = default;
  std::u16string Transliterate(std::u16string_view text) const override {
    icu::UnicodeString ustr(text.data(), text.length());
    delegate_->transliterate(ustr);
    return UnicodeStringToString16(ustr);
  }

 private:
  std::unique_ptr<icu::Transliterator> delegate_;
};

std::unique_ptr<Transliterator> CreateTransliterator(std::string_view id) {
  UParseError parseErr;
  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> delegate(
      icu::Transliterator::createInstance(
          icu::UnicodeString(id.data(), id.length()), UTRANS_FORWARD, parseErr,
          err));
  DCHECK(U_SUCCESS(err));
  DCHECK(delegate != nullptr);
  std::unique_ptr<Transliterator> result(
      new ICUTransliterator(delegate.release()));
  return result;
}
std::unique_ptr<Transliterator> CreateTransliteratorFromRules(
    std::string_view id,
    std::string_view rules) {
  UParseError parseErr;
  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> delegate(
      icu::Transliterator::createFromRules(
          icu::UnicodeString(id.data(), id.length()),
          icu::UnicodeString(rules.data(), rules.length()), UTRANS_FORWARD,
          parseErr, err));
  DCHECK(U_SUCCESS(err));
  DCHECK(delegate != nullptr);
  std::unique_ptr<Transliterator> result(
      new ICUTransliterator(delegate.release()));
  return result;
}

}  // namespace i18n
}  // namespace base

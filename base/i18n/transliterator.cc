// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/transliterator.h"

#include <stdint.h>

#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/i18n/transliterator_buildflags.h"
#if BUILDFLAG(BUILD_RUST_TRANSLIT)
#include "base/i18n/transliterator.rs.h"
#endif
#include "base/i18n/unicodestring.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#if BUILDFLAG(BUILD_RUST_TRANSLIT)
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#endif
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace base {
namespace i18n {

#if BUILDFLAG(BUILD_RUST_TRANSLIT)
class ICU4XTransliterator : public Transliterator {
 private:
  void set_locale(std::string_view locale) {
    auto t = transliterator::create_from_locale(locale.data());
    delegate_ = t.into_raw();
  }

 public:
  explicit ICU4XTransliterator(std::string_view id) {
    if (id == "Latin-ASCII") {
      set_locale("und-t-und-latn-d0-ascii");
      return;
    }
    if (id == "Katakana-Hiragana") {
      set_locale("und-Hira-t-und-kana");
      return;
    }
    if (id == "Hiragana-Katakana") {
      set_locale("und-Kana-t-und-hira");
      return;
    }
    // Change "A;B" to "::A;::B;"
    std::string rules_str;
    StringViewTokenizer t(id, ";");
    while (std::optional<std::string_view> token = t.GetNextTokenView()) {
      if (!rules_str.empty()) {
        rules_str += ";";
      }
      (rules_str += "::") += token.value();
    }
    rules_str += ";";

    delegate_ = transliterator::create_from_rules(rules_str).into_raw();
  }
  ICU4XTransliterator(std::string_view id, std::string_view rules) {
    delegate_ =
        transliterator::create_from_rules(std::string(rules)).into_raw();
  }
  ~ICU4XTransliterator() override = default;
  std::u16string Transliterate(std::u16string_view text) const override {
    rust::String result =
        transliterator::transliterate(*delegate_, UTF16ToUTF8(text));
    return UTF8ToUTF16(std::string_view(result.data(), result.length()));
  }

 private:
  raw_ptr<::transliterator::TransliteratorWrapper> delegate_;
};

std::unique_ptr<Transliterator> CreateTransliterator(std::string_view id) {
  std::unique_ptr<Transliterator> result(new ICU4XTransliterator(id));
  return result;
}
std::unique_ptr<Transliterator> CreateTransliteratorFromRules(
    std::string_view id,
    std::string_view rules) {
  std::unique_ptr<Transliterator> result(new ICU4XTransliterator(id, rules));
  return result;
}

#else
class ICUTransliterator : public Transliterator {
 public:
  explicit ICUTransliterator(icu::Transliterator* instance)
      : delegate_(instance) {}
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
#endif  // BUILDFLAG(BUILD_RUST_TRANSLIT)

}  // namespace i18n
}  // namespace base

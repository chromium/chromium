// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/transliterator.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/i18n/transliterator_buildflags.h"
#if BUILDFLAG(BUILD_RUST_TRANSLIT)
#include "base/i18n/transliterator.rs.h"
#else
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
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
    if (id.starts_with("und-t-und-x0-")) {
      set_locale(id);
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
namespace {

class ICUTransliterator : public Transliterator {
 public:
  explicit ICUTransliterator(std::unique_ptr<icu::Transliterator> delegate)
      : delegate_(std::move(delegate)) {}
  ~ICUTransliterator() override = default;
  std::u16string Transliterate(std::u16string_view text) const override {
    icu::UnicodeString ustr(text.data(), text.length());
    delegate_->transliterate(ustr);
    return UnicodeStringToString16(ustr);
  }

 private:
  std::unique_ptr<icu::Transliterator> delegate_;
};

// Legacy ICU4C copy of Chromium's custom `und-t-und-x0-*` UTS #35 rule sets
// (pre-compiled for ICU4X via `//tools/crates/icu4x_datagen/config.toml` and
// `//third_party/rust/icu4x_data/transliterators/*.txt`). Keep synchronized
// until `!BUILDFLAG(BUILD_RUST_TRANSLIT)` is removed.
std::optional<std::string_view> CustomRulesForBcp47Id(std::string_view id) {
  static constexpr auto kCustomRules =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          // Autofill default address normalization.
          {"und-t-und-x0-afill",
           "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; "
           "::Latin-ASCII;"},
          // Autofill German (DIN 5007-2) address normalization.
          {"und-t-und-x0-afillde",
           "[ö {o\u0308} Ö {O\u0308}] → oe;"
           "[ä {a\u0308} Ä {A\u0308}] → ae;"
           "[ü {u\u0308} Ü {U\u0308}] → ue;"
           "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; "
           "::Latin-ASCII;"},
          // ChromeOS input method Western European Latin diacritic stripper.
          {"und-t-und-x0-diacstr",
           "::NFC; "
           "[ á à â ä ā å ] > a; "
           "[ Á À Â Ä Ā Å ] > A; "
           "[ é è ê ë ē   ] > e; "
           "[ É È Ê Ë Ē   ] > E; "
           "[ í ì î ï ī   ] > i; "
           "[ Í Ì Î Ï Ī   ] > I; "
           "[ ó ò ô ö ō ø ] > o; "
           "[ Ó Ò Ô Ö Ō Ø ] > O; "
           "[ ú ù û ü ū   ] > u; "
           "[ Ú Ù Û Ü Ū   ] > U; "
           "[ ý ỳ ŷ ÿ ȳ   ] > y; "
           "[ Ý Ỳ Ŷ Ÿ Ȳ   ] > Y; "
           "ç > c; ñ > n; æ > ae; œ > oe; "
           "Ç > C; Ñ > N; Æ > AE; Œ > OE; "},
          // Spoof-check / ChromeOS local search diacritic & ł/ø/đ remover.
          {"und-t-und-x0-dropacc",
           "::NFD; ::[:Nonspacing Mark:] Remove; ::NFC; ł > l; ø > o; đ > d;"},
          // ChromeOS local search hyphen/dash remover.
          {"und-t-und-x0-rmhyph", "::[-~֊־᠆‐‑‒–—―⁓⁻₋−⸺⸻〜〰゠﹘﹣－] Remove;"},
          // URL spoof-check supplemental confusable mapper (when adding a
          // digit lookalike, update `digit_lookalikes_` in
          // `idn_spoof_checker.cc` as well).
          {"und-t-und-x0-xconf",
           "[æӕ] > ae; [ϼҏ] > p; [ħнћңҥӈӊԋԧԩ] > h;"
           "[ĸκкқҝҟҡӄԟ] > k; [ŋпԥกח] > n;"
           "[ŧтҭԏ七丅丆丁] > t; [ƅьҍвß] > b;  [ωшщพฟພຟ] > w;"
           "[мӎ] > m; [єҽҿၔ] > e; ґ > r; [ғӻ] > f;"
           "[ҫင] > c; [ұ丫] > y; [χҳӽӿ乂] > x;"
           "[ԃძ]  > d; [ԍဌ] > g; [ടรຣຮ] > s; ၂ > j;"
           "[०০੦૦ଠ୦೦စ] > o;"
           "[৭੧૧] > q;"
           "[џบບ] > u;"
           "[θ] > 0;"
           "[२২੨੨૨೩೭շ] > 2;"
           "[зҙӡउওਤ੩૩౩ဒვპੜკ] > 3;"
           "[੫丩ㄐ] > 4;"
           "[ճ] > 6;"
           "[৪੪୫] > 8;"
           "[૭୨౨] > 9;"
           "[—一―⸺⸻] > \\-;"},
      });
  const auto it = kCustomRules.find(base::ToLowerASCII(id));
  if (it == kCustomRules.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace

std::unique_ptr<Transliterator> CreateTransliterator(std::string_view id) {
  UParseError parse_err;
  UErrorCode err = U_ZERO_ERROR;
  icu::UnicodeString u_id(id.data(), id.length());
  std::unique_ptr<icu::Transliterator> delegate;
  if (std::optional<std::string_view> rules = CustomRulesForBcp47Id(id)) {
    delegate.reset(icu::Transliterator::createFromRules(
        u_id, icu::UnicodeString(rules->data(), rules->length()),
        UTRANS_FORWARD, parse_err, err));
  } else {
    delegate.reset(icu::Transliterator::createInstance(u_id, UTRANS_FORWARD,
                                                       parse_err, err));
  }
  DCHECK(U_SUCCESS(err));
  DCHECK(delegate != nullptr);
  if (U_FAILURE(err) || !delegate) {
    return nullptr;
  }
  return std::make_unique<ICUTransliterator>(std::move(delegate));
}

std::unique_ptr<Transliterator> CreateTransliteratorFromRules(
    std::string_view id,
    std::string_view rules) {
  UParseError parse_err;
  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> delegate(
      icu::Transliterator::createFromRules(
          icu::UnicodeString(id.data(), id.length()),
          icu::UnicodeString(rules.data(), rules.length()), UTRANS_FORWARD,
          parse_err, err));
  DCHECK(U_SUCCESS(err));
  DCHECK(delegate != nullptr);
  if (U_FAILURE(err) || !delegate) {
    return nullptr;
  }
  return std::make_unique<ICUTransliterator>(std::move(delegate));
}
#endif  // BUILDFLAG(BUILD_RUST_TRANSLIT)

}  // namespace i18n
}  // namespace base

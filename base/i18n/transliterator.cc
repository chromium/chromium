// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/transliterator.h"

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/i18n/transliterator_buildflags.h"

#if BUILDFLAG(BUILD_RUST_TRANSLIT)
#include "base/i18n/transliterator.rs.h"
#include "base/strings/string_view_rust.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/rust/cxx/v1/cxx.h"
#else
#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/i18n/unicode/translit.h"
#if UCONFIG_NO_TRANSLITERATION
#error \
    "enable_rust_translit=false requires ICU4C transliteration (UCONFIG_NO_TRANSLITERATION=0)"
#endif
#endif

namespace base {
namespace i18n {

#if BUILDFLAG(BUILD_RUST_TRANSLIT)
namespace {

class ICU4XTransliterator : public Transliterator {
 public:
  explicit ICU4XTransliterator(
      rust::Box<translit_ffi::TransliteratorWrapper> delegate)
      : delegate_(std::move(delegate)) {}
  ~ICU4XTransliterator() override = default;

  std::u16string Transliterate(std::u16string_view text) const override {
    std::string utf8_input = UTF16ToUTF8(text);
    rust::String result = translit_ffi::transliterate(
        *delegate_, StringViewToRustStrUTF8(utf8_input));
    return UTF8ToUTF16(RustStrToStringView(rust::Str(result)));
  }

 private:
  rust::Box<translit_ffi::TransliteratorWrapper> delegate_;
};

}  // namespace

std::unique_ptr<Transliterator> CreateTransliterator(std::string_view id) {
  rust::Box<translit_ffi::TransliteratorWrapper> delegate =
      translit_ffi::create_from_id(StringViewToRustSlice(id));
  if (!translit_ffi::is_valid(*delegate)) {
    DCHECK(false)
        << "Unsupported transliterator ID '" << id
        << "'; add to //tools/crates/icu4x_datagen/config.toml, run "
           "`python3 tools/crates/run_icu4x_datagen.py generate`, and (if "
           "using a legacy non-BCP-47 alias) map it in "
           "//base/i18n/translit/custom_transliterator.rs.";
    return nullptr;
  }
  return std::make_unique<ICU4XTransliterator>(std::move(delegate));
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
          // Supplemental IDN confusable skeleton mappings (`ExtraConf`).
          // Exclusively used by
          // //components/url_formatter/spoof_checks/skeleton_generator.cc
          // for URL spoof checks; do not reuse from other components.
          // Tests for these mappings live in `idn_spoof_checker_unittest.cc`
          // under //components/url_formatter/spoof_checks/ (plus a smoke test
          // in //base/i18n/transliterator_unittest.cc).
          // Keep in sync with
          // //third_party/rust/icu4x_data/transliterators/extra_conf.txt (the
          // long-term home once `BUILD_RUST_TRANSLIT` is enabled).
          // IMPORTANT: Adding a digit-lookalike? Add it to `digit_lookalikes_`
          // in //components/url_formatter/spoof_checks/idn_spoof_checker.cc,
          // too.
          //   - {U+00E6 (æ), U+04D5 (ӕ)} => "ae"
          //   - {U+03FC (ϼ), U+048F (ҏ)} => p
          //   - {U+0127 (ħ), U+043D (н), U+045B (ћ), U+04A3 (ң), U+04A5 (ҥ),
          //      U+04C8 (ӈ), U+04CA (ӊ), U+050B (ԋ), U+0527 (ԧ), U+0529 (ԩ)}
          //      => h
          //   - {U+0138 (ĸ), U+03BA (κ), U+043A (к), U+049B (қ), U+049D (ҝ),
          //      U+049F (ҟ), U+04A1 (ҡ), U+04C4 (ӄ), U+051F (ԟ)} => k
          //   - {U+014B (ŋ), U+043F (п), U+0525 (ԥ), U+0E01 (ก), U+05D7 (ח)}
          //      => n
          // TODO(crbug.com/40091387): Handle multiple skeletons for U+0525 and
          // U+0153.
          //   - {U+0167 (ŧ), U+0442 (т), U+04AD (ҭ), U+050F (ԏ), U+4E03 (七),
          //      U+4E05 (丅), U+4E06 (丆), U+4E01 (丁)} => t
          //   - {U+0185 (ƅ), U+044C (ь), U+048D (ҍ), U+0432 (в), U+00DF (ß)}
          //      => b
          //   - {U+03C9 (ω), U+0448 (ш), U+0449 (щ), U+0E1E (พ), U+0E1F (ฟ),
          //      U+0E9E (ພ), U+0E9F (ຟ)} => w
          //   - {U+043C (м), U+04CE (ӎ)} => m
          //   - {U+0454 (є), U+04BD (ҽ), U+04BF (ҿ), U+1054 (ၔ)} => e
          //   - U+0491 (ґ) => r
          //   - {U+0493 (ғ), U+04FB (ӻ)} => f
          //   - {U+04AB (ҫ), U+1004 (င)} => c
          //   - {U+04B1 (ұ), U+4E2B (丫)} => y
          //   - {U+03C7 (χ), U+04B3 (ҳ), U+04FD (ӽ), U+04FF (ӿ), U+4E42 (乂)}
          //      => x
          //   - {U+0503 (ԃ), U+10EB (ძ)} => d
          //   - {U+050D (ԍ), U+100C (ဌ)} => g
          //   - {U+0D1F (ട), U+0E23 (ร), U+0EA3 (ຣ), U+0EAE (ຮ)} => s
          //   - U+1042 (၂) => j
          //   - {U+0966 (०), U+09E6 (০), U+0A66 (੦), U+0AE6 (૦), U+0B20 (ଠ),
          //      U+0B66 (୦), U+0CE6 (೦), U+1005 (စ)} => o
          //   - {U+09ED (৭), U+0A67 (੧), U+0AE7 (૧)} => q
          //   - {U+045F (џ), U+0E1A (บ), U+0E9A (ບ)} => u
          //   - {U+03B8 (θ)} => 0
          //   - {U+0968 (२), U+09E8 (২), U+0A68 (੨), U+0AE8 (૨), U+0CE9 (೩),
          //      U+0CED (೭), U+0577 (շ)} => 2
          //   - {U+0437 (з), U+0499 (ҙ), U+04E1 (ӡ), U+0909 (उ), U+0993 (ও),
          //      U+0A24 (ਤ), U+0A69 (੩), U+0AE9 (૩), U+0C69 (౩), U+1012 (ဒ),
          //      U+10D5 (ვ), U+10DE (პ), U+0A5C (ੜ), U+10D9 (კ)} => 3
          //   - {U+0A6B (੫), U+4E29 (丩), U+3110 (ㄐ)} => 4
          //   - U+0573 (ճ) => 6
          //   - {U+09EA (৪), U+0A6A (੪), U+0B6B (୫)} => 8
          //   - {U+0AED (૭), U+0B68 (୨), U+0C68 (౨)} => 9
          //   Map a few dashes that ICU doesn't map. These are already blocked
          //   by ICU, but mapping them allows us to detect same skeletons.
          //   - {U+2014 (—), U+4E00 (一), U+2015 (―), U+2E3A (⸺), U+2E3B (⸻)}
          //      => -
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
#endif  // BUILDFLAG(BUILD_RUST_TRANSLIT)

}  // namespace i18n
}  // namespace base

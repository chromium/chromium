// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/test/locales_for_test.h"

#include <vector>

#include "base/check_is_test.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"

namespace base::i18n {
namespace {

constexpr auto kLocales = std::to_array<std::string_view>(
    {"aa",     "ab",  "af",    "ak",    "am",    "an",    "ar",    "as",
     "ast",    "av",  "ay",    "az-az", "az-ir", "ba",    "be",    "ber-dz",
     "ber-ma", "bg",  "bh",    "bho",   "bi",    "bin",   "bm",    "bn",
     "bo",     "br",  "brx",   "bs",    "bua",   "byn",   "ca",    "ce",
     "ch",     "chm", "chr",   "co",    "crh",   "cs",    "csb",   "cu",
     "cv",     "cy",  "da",    "de",    "doi",   "dv",    "dz",    "ee",
     "el",     "en",  "eo",    "es",    "et",    "eu",    "fa",    "fat",
     "ff",     "fi",  "fil",   "fj",    "fo",    "fr",    "fur",   "fy",
     "ga",     "gd",  "gez",   "gl",    "gn",    "gu",    "gv",    "ha",
     "haw",    "he",  "hi",    "hne",   "ho",    "hr",    "ht",    "hu",
     "hy",     "hz",  "ia",    "id",    "ie",    "ig",    "ii",    "ik",
     "io",     "is",  "it",    "iu",    "ja",    "jv",    "ka",    "kaa",
     "kab",    "ki",  "kj",    "kk",    "kl",    "km",    "kn",    "ko",
     "kok",    "kr",  "ks",    "ku-am", "ku-iq", "ku-ir", "ku-tr", "kum",
     "kv",     "kw",  "kwm",   "ky",    "la",    "lah",   "lb",    "lez",
     "lg",     "li",  "ln",    "lo",    "lt",    "lv",    "mai",   "mg",
     "mh",     "mi",  "mk",    "ml",    "mn-cn", "mn-mn", "mni",   "mo",
     "mr",     "ms",  "mt",    "my",    "na",    "nb",    "nds",   "ne",
     "ng",     "nl",  "nn",    "no",    "nr",    "nso",   "nv",    "ny",
     "oc",     "om",  "or",    "os",    "ota",   "pa",    "pa-pk", "pap-an",
     "pap-aw", "pl",  "ps-af", "ps-pk", "pt",    "qu",    "quz",   "rm",
     "rn",     "ro",  "ru",    "rw",    "sa",    "sah",   "sat",   "sc",
     "sco",    "sd",  "se",    "sel",   "sg",    "sh",    "shs",   "si",
     "sid",    "sk",  "sl",    "sm",    "sma",   "smj",   "smn",   "sms",
     "sn",     "so",  "sq",    "sr",    "ss",    "st",    "su",    "sv",
     "sw",     "syr", "ta",    "te",    "tg",    "th",    "ti-er", "ti-et",
     "tig",    "tk",  "tl",    "tn",    "to",    "tr",    "ts",    "tt",
     "tw",     "ty",  "tyv",   "ug",    "uk",    "ur",    "uz",    "ve",
     "vi",     "vo",  "vot",   "wa",    "wal",   "wen",   "wo",    "xh",
     "yap",    "yi",  "yo",    "za",    "zh-cn", "zh-hk", "zh-mo", "zh-sg",
     "zh-tw",  "zu"});
}  // namespace

std::vector<LanguageTag> GetLocalesForTest() {
  CHECK_IS_TEST();
  std::vector<LanguageTag> output;
  for (std::string_view locale : kLocales) {
    if (std::optional<LanguageTag> locale_tag =
            GetLanguageTagFromString(locale)) {
      output.push_back(*locale_tag);
    }
  }
  return output;
}

}  // namespace base::i18n

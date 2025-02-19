// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/generative_ai_country_restrictions.h"

#include "base/containers/fixed_flat_set.h"

namespace ash {

namespace {
constexpr auto kCountryAllowlist = base::MakeFixedFlatSet<std::string_view>(
    {"ae", "ag", "ai", "am", "ao", "aq", "ar", "as", "at", "au", "aw", "az",
     "bb", "bd", "be", "bf", "bg", "bh", "bi", "bj", "bl", "bm", "bn", "bo",
     "bq", "br", "bs", "bt", "bw", "bz", "ca", "cc", "cd", "cf", "cg", "ch",
     "ci", "ck", "cl", "cm", "co", "cr", "cv", "cw", "cx", "cy", "cz", "de",
     "dj", "dk", "dm", "do", "dz", "ec", "ee", "eg", "eh", "er", "es", "et",
     "fi", "fj", "fk", "fm", "fr", "ga", "gb", "gd", "ge", "gg", "gh", "gi",
     "gm", "gn", "gq", "gr", "gs", "gt", "gu", "gw", "gy", "hm", "hn", "hr",
     "ht", "hu", "id", "ie", "il", "im", "in", "io", "iq", "is", "it", "je",
     "jm", "jo", "jp", "ke", "kg", "kh", "ki", "km", "kn", "kr", "kw", "ky",
     "kz", "la", "lb", "lc", "li", "lk", "lr", "ls", "lt", "lu", "lv", "ly",
     "ma", "mg", "mh", "ml", "mn", "mp", "mr", "ms", "mt", "mu", "mv", "mw",
     "mx", "my", "mz", "na", "nc", "ne", "nf", "ng", "ni", "nl", "no", "np",
     "nr", "nu", "nz", "om", "pa", "pe", "pg", "ph", "pk", "pl", "pm", "pn",
     "pr", "ps", "pt", "pw", "py", "qa", "ro", "rw", "sa", "sb", "sc", "sd",
     "se", "sg", "sh", "si", "sk", "sl", "sn", "so", "sr", "ss", "st", "sv",
     "sz", "tc", "td", "tg", "th", "tj", "tk", "tl", "tm", "tn", "to", "tr",
     "tt", "tv", "tw", "tz", "ug", "um", "us", "uy", "uz", "vc", "ve", "vg",
     "vi", "vn", "vu", "wf", "ws", "ye", "za", "zm", "zw"});
}

bool IsGenerativeAiAllowedForCountry(std::string_view country_code) {
  return kCountryAllowlist.contains(country_code);
}

std::vector<std::string> GetGenerativeAiCountryAllowlist() {
  return {kCountryAllowlist.begin(), kCountryAllowlist.end()};
}

}  // namespace ash
